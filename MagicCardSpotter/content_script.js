var gSettings = undefined;
var gCurrentMode = "disabled";
var cExpectedVideoHeight = 720;
var gDebug = false;

class VideoDetection {
	constructor(video) {
		this.video = video;
		this.autoSearchStartTime = performance.now();
		this.autoPostMessageTime = performance.now();
		this.videoCopy = document.createElement("canvas");
		this.searchWorker = new Worker(chrome.extension.getURL("worker.js"));

		let _this = this;
		this.searchWorker.onmessage = function (e) {

			if (e.data.result == undefined)
			{
				_this.ResetAutoSearch();
				_this.ResetSearch();
				return;
			}

			if (e.data.result.isautomatch)
			{
				_this.ResetAutoSearch();
			}

			if (!e.data.result.isautomatch)
			{
				_this.ResetSearch();
			}

			if (gDebug && !e.data.result.isautomatch)
			{
				let sendTime = _this.autoPostMessageTime - _this.autoSearchStartTime;
				let now = performance.now();
				let searchTime = now - _this.autoPostMessageTime;
				console.log("SendTime: " + sendTime.toString() + ", SearchTime: " + searchTime.toString());
			}
		
			ProcessResult(e.data.result);
		}

		this.searchStartTime = performance.now();
		this.postMessageTime = performance.now();

		this.searchState = "done";
	}

	PostScreenshot() {

		if (!gSettings.autoscreen) {
			console.log("!gSettings.autoscreen")
			return;
		}

		if (gCurrentMode == "disabled") {
			this.ResetAutoSearch();
			return;
		}

		if (this.searchState != "done") {
			this.ResetAutoSearch();
			return;
		}

		this.searchState = "autoSearch";
		this.autoSearchStartTime = performance.now();
		if (this.videoCopy == null) {
			this.videoCopy = document.createElement("canvas");
		}

		var areaWidth = gSettings.automatchwidth;
		var areaHeight = gSettings.automatchheight;
		var xStart = gSettings.automatchx;
		var yStart = gSettings.automatchy;
		var scale = cExpectedVideoHeight / this.video.videoHeight;
		this.videoCopy.height = this.video.videoHeight * scale * areaHeight;
		this.videoCopy.width = this.video.videoWidth * scale * areaWidth;

		var copyContext = this.videoCopy.getContext('2d');
		copyContext.clearRect(0, 0, this.videoCopy.width, this.videoCopy.height);
		copyContext.drawImage(this.video, this.video.videoWidth * xStart, this.video.videoHeight * yStart, this.video.videoWidth * areaWidth, this.video.videoHeight * areaHeight, 0, 0, this.videoCopy.width, this.videoCopy.height);

		const imageData = copyContext.getImageData(0, 0, this.videoCopy.width, this.videoCopy.height);
		const uintArray = imageData.data;
		this.searchWorker.postMessage({ action: 'ADD_SCREEN', width: this.videoCopy.width, height: this.videoCopy.height, imageData: uintArray }, [uintArray.buffer]);

		this.autoPostMessageTime = performance.now();
	}

	Search(mx, my) {
		this.searchStartTime = performance.now();

		var { rx, ry } = mouseToVideoRelativeCoords(mx, my);

		var sx = rx * this.video.videoWidth;
		var sy = ry * this.video.videoHeight;

		this.searchState = "search";
		document.body.style.cursor = "wait";

		var cTimeoutValue = 5000;

		this.searchTimeout = setTimeout(this.ResetSearch, 5000);

		ImageSearchVideoCopy(this.videoCopy, this.video, sx, sy);

		const imageData = this.videoCopy.getContext('2d').getImageData(0, 0, this.videoCopy.width, this.videoCopy.height);
		const uintArray = imageData.data;
		this.searchWorker.postMessage({ action: 'FIND_CARD', width: this.videoCopy.width, height: this.videoCopy.height, imageData: uintArray, x: sx, y: sy }, [uintArray.buffer]);

		showTooltipSearchImage(sx, sy);

		this.postMessageTime = performance.now();
	}

	ResetSearch() {
		clearTimeout(this.searchTimeout);
		this.searchState = "done";
		document.body.style.cursor = "auto";
	}

	ResetAutoSearch() {
		if (this.searchState == "autoSearch") {
			this.searchState = "done";
		}
		
		let _this = this;
		this.screenTimeout = setTimeout(function(){_this.PostScreenshot();}, 250);
	}

	SetSetting(k, v) {
		this.searchWorker.postMessage({ action: 'SET_SETTING', key: k, value: v });
	}
}

var gTriggerSearchTimeout;

var gUpdateTimeout;

var gx = 0;
var gy = 0;

var lastGx = 0;
var lastGy = 0;

var cUnblockSettingsWidth = 200;//Change this to be max cap based on client size (not fixed offset)

var cDetailsBaseWidth = 224;
var cDetailsBaseHeight = 324;
var cDetailsImgHeight = 308;

var resultHistory = [];


var fullScreen = false;

var lastresultstime = performance.now();

var divIndex = "2147483646";
var extensionUrl = chrome.extension.getURL('');

var cardSpotterLibSettings = [
	"automatchhistorysize",
	"cardpool",
	"mincardsize",
	"maxcardsize",
	"okscore",
	"goodscore"
];

function UpdateSettings(callback) {
	var wasAutoscreen = gSettings != undefined && gSettings.autoscreen != undefined && gSettings.autoscreen; //misses if we enable/disable on the same video
	chrome.storage.sync.get(null, function (items) {
		gSettings = items;
		updateAutoScreenHighlight();
		updateMouseSearchHighlight();
		if (gSettings.autoscreen && !wasAutoscreen) {

			videoSearchers.forEach(function (vs) {
				vs.PostScreenshot();
			});
		}

		for (var key in items) {
			if (cardSpotterLibSettings.indexOf(key) != -1) {

				videoSearchers.forEach(function (vs) {
					vs.SetSetting(key, items[key].toString());
				});
			}
		}

		UpdateDebugColors();

		if (callback !== undefined) {
			callback();
		}
	});
}

function UpdateDebugColors() {
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	if (mousegrabcanvas != null) {
		if (gSettings.canvascolor) {
			mousegrabcanvas.style.backgroundColor = "rgba(122,255,122,0.5)";
		}
		else {
			mousegrabcanvas.style.backgroundColor = "rgba(0,0,0,0)";
		}
	}
	var autocolor = document.getElementById("autocolor");
	var showColor = gSettings.autocolor && !IsFullScreen();
	if (showColor) {
		if (autocolor == null) {
			autocolor = document.createElement("canvas");
			autocolor.style.backgroundColor = "rgba(122,122,255,0.5)";
			autocolor.id = "autocolor";
			autocolor.style.zIndex = "2147483647";
			autocolor.style.position = "absolute";
			document.body.insertBefore(autocolor, document.body.firstChild);
		}
		var videoRect = getVideoClientRect();
		var areaWidth = gSettings.automatchwidth * videoRect.width;
		var areaHeight = gSettings.automatchheight * videoRect.height;
		var xStart = gSettings.automatchx * videoRect.width;
		var yStart = gSettings.automatchy * videoRect.height;
		autocolor.style.top = (videoRect.top + yStart).toString() + "px";
		autocolor.style.left = (videoRect.left + xStart).toString() + "px";
		autocolor.width = areaWidth;
		autocolor.height = areaHeight;
	}
	else if (autocolor != null) {
		autocolor.parentNode.removeChild(autocolor);
	}
}

function saveHistory() {
	var miniHistory = [];
	for (var i = 0; i < resultHistory.length; i++) {
		var result = resultHistory[i];
		miniHistory.push({ name: result.name, set: result.setcode, price: result.price });
	}

	var _myArray = JSON.stringify(miniHistory, null, 4); //indentation in json format, human readable

	var vLink = document.createElement('a'),
		vBlob = new Blob([_myArray], { type: "octet/stream" }),
		vName = 'matchHistory.json',
		vUrl = window.URL.createObjectURL(vBlob);
	vLink.setAttribute('href', vUrl);
	vLink.setAttribute('download', vName);
	vLink.click();
}

function Update() {
	if (gCurrentMode == "disabled") {
		return;
	}

	UpdateCanvasSize();
	UpdateSettings(function () { gUpdateTimeout = setTimeout(Update, 500); });
}


function setPixel(imageData, x, y, r, g, b, a) {
	index = (x + y * imageData.width) * 4;
	imageData.data[index + 0] = r;
	imageData.data[index + 1] = g;
	imageData.data[index + 2] = b;
	imageData.data[index + 3] = a;
}

function ClearTooltipDiv() {
	var tooltipDiv = document.getElementById("tooltipDiv");
	if (tooltipDiv != null) {
		while (tooltipDiv.firstChild) {
			tooltipDiv.removeChild(tooltipDiv.firstChild);
		}
	}
}

function IsFullScreen() {
	return toolTipParent.scrollWidth > screen.width - 30;
}

var lastVideoRect = null;

function getVideoClientRect() {
	var rect = toolTipParent.getBoundingClientRect();
	const xScale = GetClientToVideoScaleX();
	const yScale = GetClientToVideoScaleY();
	if (xScale < yScale) //div is wider than high
	{
		const actualWidth = toolTipParent.videoWidth / yScale;//even scale on video so it will use the larger one
		const diff = rect.width - actualWidth;
		rect.x = rect.x + (diff / 2.0);
		rect.width = actualWidth;
	}
	return rect;
}

function UpdateCanvasSize() {
	var videoRect = getVideoClientRect();

	if (lastVideoRect == null || lastVideoRect.top != videoRect.top || lastVideoRect.left != videoRect.left || lastVideoRect.width != videoRect.width || lastVideoRect.height != videoRect.height) {
		lastVideoRect = videoRect;

		var nowFullscreen = IsFullScreen();

		if (fullScreen != nowFullscreen) {
			fullScreen = nowFullscreen;

			var mousegrabcanvas = document.getElementById("mousegrabcanvas");
			if (mousegrabcanvas.parentNode != undefined) {
				mousegrabcanvas.parentNode.removeChild(mousegrabcanvas);
			}

			InsertMouseGrabCanvas(mousegrabcanvas);
		}

		UpdateMouseCanvasSize();
	}
}

function UpdateMouseCanvasSize() {
	var videoRect = getVideoClientRect();
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	mousegrabcanvas.style.position = "absolute";
	mousegrabcanvas.width = (videoRect.width * (gSettings.clickareawidth - gSettings.clickareax));
	if (mousegrabcanvas.onmouseover != null) {
		mousegrabcanvas.width -= cUnblockSettingsWidth;
	}
	mousegrabcanvas.height = videoRect.height * (gSettings.clickareaheight - gSettings.clickareay);
	mousegrabcanvas.style.top = (videoRect.top + videoRect.height * gSettings.clickareay).toString() + "px";
	mousegrabcanvas.style.left = (videoRect.left + videoRect.width * gSettings.clickareax).toString() + "px";
	mousegrabcanvas.style.width = (mousegrabcanvas.width).toString() + "px";
	mousegrabcanvas.style.height = (mousegrabcanvas.height).toString() + "px";
	mousegrabcanvas.style.zIndex = divIndex;
}

function InsertMouseGrabCanvas(mousegrabcanvas) {
	if (IsFullScreen()) {
		toolTipParent.parentNode.insertBefore(mousegrabcanvas, toolTipParent.parentNode.firstChild);
	}
	else {
		document.body.insertBefore(mousegrabcanvas, document.body.firstChild);
	}
}

function GetClientToVideoScaleX() {
	return toolTipParent.videoWidth / toolTipParent.clientWidth;
}
function GetClientToVideoScaleY() {
	return toolTipParent.videoHeight / toolTipParent.clientHeight;
}

function mouseToVideoRelativeCoords(mx, my) {
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	var canvasRect = mousegrabcanvas.getBoundingClientRect();
	var rx = mx / canvasRect.width;
	var ry = my / canvasRect.height;
	rx *= (gSettings.clickareawidth - gSettings.clickareax);
	rx += gSettings.clickareax;
	ry *= (gSettings.clickareaheight - gSettings.clickareay);
	ry += gSettings.clickareay;
	return { rx, ry };
}

function showTooltipSearchImage(sx, sy) {
	var tooltipImage = document.getElementById("TooltipImage");
	if (tooltipImage != null) {
		var debugImage = document.getElementById("debugImage");
		if (debugImage == null) {
			debugImage = document.createElement("canvas");
			debugImage.id = "debugImage";
			debugImage.height = tooltipImage.height;
			debugImage.width = tooltipImage.width - 16;
		}
		if (debugImage != null) {
			ImageSearchVideoCopy(debugImage, toolTipParent, sx, sy);
			var searchImage = tooltipImage.cloneNode(false);
			searchImage.id = "searchImage";
			searchImage.src = debugImage.toDataURL("image/png");
			tooltipImage.parentNode.appendChild(searchImage);
			tooltipImage.parentNode.removeChild(tooltipImage);
		}
	}
}

function OnMouseStopped() {
	lastGx = gx;
	lastGy = gy;
	Search(gx, gy);
}

function UseMouseOver() {
	if (fullScreen || gSettings.mousemode == "mouseover") {
		var tooltipDiv = document.getElementById("tooltipDiv");
		if (tooltipDiv != null && tooltipDiv.style.visibility == 'visible') {
			var tooltipRect = tooltipDiv.getBoundingClientRect();
			var mousegrabcanvas = document.getElementById("mousegrabcanvas");
			var canvasRect = mousegrabcanvas.getBoundingClientRect();

			var mx = gx + canvasRect.left;
			var my = gy + canvasRect.top;
			if (tooltipRect.left < mx && tooltipRect.right > mx && tooltipRect.top < my && tooltipRect.bottom > my) {
				return false;
			}
		}
		return true;
	}
	return false;
}

function UseMouseClick() {
	return !fullScreen && gSettings.mousemode == "click";
}

function OnMouseMove(e) {
	gx = e.offsetX;
	gy = e.offsetY;

	clearTimeout(gTriggerSearchTimeout);

	var dx = lastGx - gx;
	var dy = lastGy - gy;

	if (UseMouseOver()) {
		if ((dx * dx + dy * dy) > (15 * 15)) {
			gTriggerSearchTimeout = setTimeout(OnMouseStopped, 50);
		}
	}
}

function OnMouseDown(e) {
	if (UseMouseClick()) {
		gx = e.offsetX;
		gy = e.offsetY;
		lastGx = gx;
		lastGy = gy;
		videoSearchers[0].Search(gx, gy);
	}
}

function OnMouseOver() {
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	mousegrabcanvas.onmouseover = null;
	mousegrabcanvas.onmousemove = OnMouseMove;
	mousegrabcanvas.onmousedown = OnMouseDown;
	mousegrabcanvas.onmouseout = OnMouseOut;
	UpdateMouseCanvasSize();
}

function OnMouseOut() {
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	mousegrabcanvas.onmouseover = OnMouseOver;
	mousegrabcanvas.onmousemove = null;
	mousegrabcanvas.onmousedown = null;
	mousegrabcanvas.onmouseout = null;
	UpdateMouseCanvasSize();
	clearTimeout(gTriggerSearchTimeout);
}

function clearState() {
	clearTimeout(gTriggerSearchTimeout);
	document.body.style.cursor = "auto";
	gSearchState = "done";
}
function Length(vec) {
	return Math.sqrt(vec.x * vec.x + vec.y * vec.y);
}

function RotateVector(vec, deg) {
	var rad = deg * (Math.PI / 180)
	return { x: vec.x * Math.cos(rad) - vec.y * Math.sin(rad), y: vec.x * Math.sin(rad) + vec.y * Math.cos(rad) };
}

function ImageSearchVideoCopy(canvas, video, px, py) {
	var scale = cExpectedVideoHeight / video.videoHeight;
	canvas.height = scale * video.videoHeight * Math.min(1.0, (gSettings.maxcardsize / 100.0) * 2.0);
	canvas.width = canvas.height;

	var copyContext = canvas.getContext('2d');
	copyContext.clearRect(0, 0, canvas.width, canvas.height);
	copyContext.drawImage(video, px - canvas.width * 0.5 / scale, py - canvas.height * 0.5 / scale, canvas.width / scale, canvas.height / scale, 0, 0, canvas.width, canvas.height);
}

function updateMouseSearchHighlight() {
	var mini = document.getElementById("mini");
	if (mini != null) {
		if (!gSettings.mousemodeenabled) {
			var mousegrabcanvas = document.getElementById("mousegrabcanvas");
			mousegrabcanvas.style.zIndex = -1;
			mini.classList.remove('mdi-light');
		}
		else {
			var mousegrabcanvas = document.getElementById("mousegrabcanvas");
			mousegrabcanvas.style.zIndex = divIndex;
			mini.classList.add('mdi-light');
		}
	}

}

function updateAutoScreenHighlight() {
	var csrenew = document.getElementById("csrenew");
	if (csrenew != null) {
		if (!gSettings.autoscreen) {
			csrenew.classList.remove('mdi-light');
		}
		else {
			csrenew.classList.add('mdi-light');
		}
	}
}

function CreateMenu() {
	var namediv = document.getElementById("namediv");

	var bottom = document.createElement("div");
	bottom.classList.add("CardSpotterBottom");
	namediv.appendChild(bottom);

	var menuDiv = document.createElement("div");
	menuDiv.id = "CardSpotterMenu";
	menuDiv.classList.add("CardSpotterMenu");
	bottom.appendChild(menuDiv);

	var mini = document.createElement("i");
	mini.id = "mini";
	mini.classList.add("mdi", "mdi-mouse", "mdi-18px");

	updateMouseSearchHighlight();

	mini.onclick = function () {
		chrome.storage.sync.set(
			{
				mousemodeenabled: !gSettings.mousemodeenabled

			}, updateMouseSearchHighlight);
	}
	menuDiv.appendChild(mini);

	var csrenew = document.createElement("i");
	csrenew.id = "csrenew";
	csrenew.classList.add("mdi", "mdi-autorenew", "mdi-18px");
	csrenew.title = "Toggle automatic tooltip";
	updateAutoScreenHighlight();

	csrenew.onclick = function () {
		if (!gSettings.autoscreen)
			PostScreenshot();

		chrome.storage.sync.set(
			{
				autoscreen: !gSettings.autoscreen

			}, updateAutoScreenHighlight);
	}

	menuDiv.appendChild(csrenew);

	var cssettings = document.createElement("i");
	cssettings.id = "cssettings";
	cssettings.title = "Options";
	cssettings.classList.add("mdi", "mdi-settings", "mdi-18px", "mdi-flip-horizontal");
	cssettings.onclick = function () { window.open(extensionUrl + "options.html"); }
	menuDiv.appendChild(cssettings);


	if (gSettings.showsavebutton) {
		var cssave = document.createElement("i");
		cssave.id = "cssave";
		cssave.title = "Save match history to file.";
		cssave.classList.add("mdi", "mdi-content-save", "mdi-18px", "mdi-flip-horizontal");
		cssave.onclick = function () { saveHistory(); }
		menuDiv.appendChild(cssave);
	}
}

function Setup() {
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	if (mousegrabcanvas == null) {
		mousegrabcanvas = document.createElement("canvas");
		mousegrabcanvas.id = "mousegrabcanvas";
		mousegrabcanvas.style.position = "absolute";
		mousegrabcanvas.style.top = "0px";
		mousegrabcanvas.style.left = "0px";
		InsertMouseGrabCanvas(mousegrabcanvas);
	}

	var namediv = document.getElementById("namediv");
	if (namediv == null) {
		namediv = document.createElement("div");
		namediv.id = "namediv";

		if (toolTipParent != null)//fullScreen)
		{
			toolTipParent.parentNode.insertBefore(namediv, toolTipParent.parentNode.firstChild);
		}
		else {
			document.body.insertBefore(namediv, document.body.firstChild);
		}
		CreateMenu();

		var menudiv = document.getElementById("CardSpotterMenu");
		menudiv.className = "CardSpotterMenu CardSpotterMenuBig";
	}
}

function Teardown() {
	clearTimeout(gUpdateTimeout);
//	chrome.runtime.onMessage.removeListener(handlePopupOrBackgroundMessage);
	var namediv = document.getElementById("namediv");
	if (namediv != null) {
		namediv.parentNode.removeChild(namediv);
	}
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	if (mousegrabcanvas != null) {
		mousegrabcanvas.parentNode.removeChild(mousegrabcanvas);
	}
	var tooltipDiv = document.getElementById("tooltipDiv");
	if (tooltipDiv != null) {
		tooltipDiv.parentNode.removeChild(tooltipDiv);
	}

	lastVideoRect = null;
}

function SetMode(aMode) {

	if (gCurrentMode == aMode) {
		return;
	}

	clearState();

	if (aMode == "disabled") {
		gCurrentMode = aMode;
		Teardown();
		return;
	}

	if (gCurrentMode == "disabled") {
		Setup();
		gCurrentMode = aMode;
		Update();
	}

	gCurrentMode = aMode;

	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	mousegrabcanvas.onmouseover = OnMouseOver;
}

function SetCanvasImage(inputCanvas, request) {
	var view = new Uint8Array(request.imgdata);

	var height = request.height;
	var width = request.width;
	var channels = request.channels;

	inputCanvas.width = width;
	inputCanvas.height = height;

	var context = inputCanvas.getContext("2d");
	context.fillStyle = "green";
	context.fillRect(0, 0, width, height);

	var imageData = context.createImageData(width, height);
	for (y = 0; y < height; y++) {
		var rowStart = y * width * channels;
		for (x = 0; x < width; x++) {
			var b = view[rowStart + x * channels + 0];
			var g = view[rowStart + x * channels + 1];
			var r = view[rowStart + x * channels + 2];
			if (r == undefined || g == undefined || b == undefined) {
				continue;
			}
			setPixel(imageData, x, y, r, g, b, 255); // 255 opaque
		}
	}
	context.putImageData(imageData, 0, 0); // at coords 0,0
}

var imageCache = {};

function getImage(multiverseid, imageurl, usename, cardName, callback) {
	var img = imageCache[imageurl];
	if (img == undefined || usename) {
		img = new Image();
		img.src = imageurl;
		img.width = 224;
		img.height = 313;
		imageCache[imageurl] = img;
	}

	callback(img);
}

function TooltipAddText(list, text) {
	var entry = document.createElement("li");
	entry.className = "CardSpotter";
	entry.appendChild(document.createTextNode(text));
	list.appendChild(entry);
	return entry;
}

function TooltipAddLink(list, url, title, text) {
	var entry = document.createElement("li");
	entry.className = "CardSpotter";
	var link = document.createElement("a");
	link.className = "CardSpotter";
	link.href = url;
	link.target = "_blank";
	link.title = title;
	link.appendChild(document.createTextNode(text));
	entry.appendChild(link);
	list.appendChild(entry);
}

function TooltipCreateBigImageLi() {
	var list = document.getElementById("TooltipList");
	var entry = document.createElement("li");
	entry.className = "CardSpotter";
	entry.style.height = cDetailsImgHeight.toString() + "px";
	list.appendChild(entry);
	return entry;
}

function TooltipAddQualityItem(list, scoreNumber) {
	var confidence = Math.round(100 - ((100 * scoreNumber) / 1024));
	if (confidence > 98) {
		return;
	}

	var entry = document.createElement("li");
	entry.className = "CardSpotter";
	entry.innerText = confidence.toString() + "%";
	list.appendChild(entry);

	return entry;
}

function drawRect(rect, color) {
	var box = {};
	box.minx = Math.min(rect[0].x, rect[1].x, rect[2].x, rect[3].x);
	box.maxx = Math.max(rect[0].x, rect[1].x, rect[2].x, rect[3].x);
	box.miny = Math.min(rect[0].y, rect[1].y, rect[2].y, rect[3].y);
	box.maxy = Math.max(rect[0].y, rect[1].y, rect[2].y, rect[3].y);

	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	var manualContext = mousegrabcanvas.getContext("2d");
	//RECTS ARE IN VIDEOSPACE?
	manualContext.clearRect(0, 0, mousegrabcanvas.width, mousegrabcanvas.height);
	manualContext.beginPath();
	manualContext.moveTo(rect[0].x, rect[0].y);
	manualContext.lineTo(rect[1].x, rect[1].y);
	manualContext.lineTo(rect[2].x, rect[2].y);
	manualContext.lineTo(rect[3].x, rect[3].y);
	manualContext.lineTo(rect[0].x, rect[0].y);
	manualContext.lineWidth = 6;
	manualContext.strokeStyle = color;
	manualContext.stroke();
}

var interval;
function fadeOutRectangle(rect) {
	var steps = 15;
	var fadesteps = 10;
	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	var manualContext = mousegrabcanvas.getContext("2d");

	manualContext.clearRect(0, 0, mousegrabcanvas.width, mousegrabcanvas.height);
	clearInterval(interval);
	i = 0, // step counter
		interval = setInterval(function () {
			var color = 'rgba(50,50,255,' + ((fadesteps - Math.max(i + fadesteps - steps, 0)) / fadesteps - 1 / fadesteps) + ')';
			drawRect(rect, color);
			i++;
			if (i === steps) { // stop if done
				var mousegrabcanvas = document.getElementById("mousegrabcanvas");
				var manualContext = mousegrabcanvas.getContext("2d");

				manualContext.clearRect(0, 0, mousegrabcanvas.width, mousegrabcanvas.height);
				clearInterval(interval);
			}
		}, 100);
}

function BlinkCardRect(result) {
	var rect = [{ x: result.px0, y: result.py0 }, { x: result.px1, y: result.py1 }, { x: result.px2, y: result.py2 }, { x: result.px3, y: result.py3 }];
	fadeOutRectangle(rect);
}

function getImageUrl(coreUrl) {
	var cUseLocalImages = false;
	if (cUseLocalImages) {
		return extensionUrl + "images/" + coreUrl + ".png";
	}
	var useFast = true;
	if (useFast) {
		return "https://img.scryfall.com/cards/normal/" + coreUrl + ".jpg";
	}
	return "https://img.scryfall.com/cards/png/" + coreUrl + ".png";
}

function ShowResult(result) {
	CreateTooltipDiv();

	var id = result.id;
	var cardName = result.name;
	var setcode = result.setcode;
	var imageurl = getImageUrl(result.url);
	var score = result.score;

	var tooltipDiv = document.getElementById("tooltipDiv");
	if (result.isautomatch)
	{
		BlinkCardRect(result);
	}

	if (gSettings.tooltiplogo) {
		CreateNameBar(tooltipDiv);
	}

	var imgEntry = TooltipCreateBigImageLi();

	if (id != undefined) {
		var usename = id < 0;
		getImage(id, imageurl, usename, cardName, function (img) {
			img.className = "CardSpotter";
			img.id = "TooltipImage";
			imgEntry.appendChild(img);

			function sameName(listRes) {
				return listRes.name == result.name;
			}

			if (resultHistory.find(sameName) == undefined) {
				resultHistory.push(result);
			}
		});
	}

	if (score <= 0) {
		TooltipAddText(document.getElementById("TooltipList"), cardName);
		return;
	}

	if (gSettings.debugview) {
		var path = (result.path == undefined) ? "?" : result.path;
		var sendTime = (result.isautomatch) ? autoPostMessageTime - autoSearchStartTime : postMessageTime - searchStartTime;
		var now = performance.now();
		var searchTime = now - ((result.isautomatch) ? autoPostMessageTime : postMessageTime);
		TooltipAddText(document.getElementById("TooltipList"), "SendTime: " + sendTime.toString() + ", SearchTime: " + searchTime.toString() + ", Score: " + score.toString() + ", Path: " + path.toString());
	}

	if (gSettings.tooltipscryfall) {
		var tooltipList = document.getElementById("TooltipList");
		var li = document.createElement("li");
		var linkList = document.createElement("ul");
		li.appendChild(linkList);
		linkList.className = "CardSpotter";
		linkList.id = "LinkList";
		tooltipList.appendChild(li);

		TooltipAddQualityItem(linkList, score);
		TooltipAddLink(linkList, "https://scryfall.com/card/" + encodeURIComponent(setcode) + "/" + id, "Scryfall", "Scryfall");
	}

	var tooltipDiv = document.getElementById("tooltipDiv");
	tooltipDiv.onmouseover = function () {
		BlinkCardRect(result);
		tooltipDiv.onmouseover = null; //TODO: this prevents flicker
		var historyList = document.getElementById("HistoryList");
		while (historyList.firstChild) {
			historyList.removeChild(historyList.firstChild);
		}

		if (gSettings.showhistory) {
			PopulateHistoryList(historyList);
		}
	}
}

function PopulateHistoryList(historyList) {
	var row;
	var c = 0;
	function showHistoricalResult(h) { return function () { var r2 = resultHistory[h]; ShowResult(r2); }; }
	;
	for (var i = Math.max(0, resultHistory.length - 10); i < resultHistory.length; i++) {
		if (c % 5 == 0) {
			row = document.createElement("li");
			historyList.appendChild(row);
		}
		var r = resultHistory[i];
		var img = imageCache[getImageUrl(r.url)];
		if (img != undefined) {
			var historyImg = img.cloneNode(false);
			historyImg.className = "HistoryList";
			historyImg.onmousedown = showHistoricalResult(i);
			row.appendChild(historyImg);
			c += 1;
		}
	}
	historyList.style.visibility = 'visible';
}

function CreateNameBar(tooltipDiv) {
	var list = document.getElementById("TooltipList");
	var entry = document.createElement("li");
	entry.className = "CardSpotter Move";
	list.appendChild(entry);
	var img = new Image();
	img.src = extensionUrl + "cardspottername.png";
	img.className = "CardSpotter";
	img.id = "CardSpotterName";
	entry.appendChild(img);
	entry.onmousedown = function (ed) {
		var cachedOffsetX = gSettings.offsetx;
		var cachedOffsetY = gSettings.offsety;
		document.onmousemove = function (e) {
			var dx = e.movementX;
			var dy = e.movementY;
			cachedOffsetX += dx;
			cachedOffsetY += dy;
			var transformScale = gSettings.detailsTargetWidth / cDetailsBaseWidth;
			tooltipDiv.style.transform = "Translate(" + cachedOffsetX + "px," + cachedOffsetY + "px) Scale(" + transformScale.toString() + ")";
		};
		document.onmouseup = function () {
			document.onmouseup = null;
			document.onmousemove = null;
			chrome.storage.sync.set({
				offsetx: cachedOffsetX,
				offsety: cachedOffsetY
			}, function () { });
		};
	};
}

function CreateTooltipDiv() {
	var tooltipDiv = document.getElementById("tooltipDiv");
	if (tooltipDiv != null)//insanity
	{
		tooltipDiv.parentNode.removeChild(tooltipDiv);
		tooltipDiv = null;
	}

	if (tooltipDiv == null) {
		tooltipDiv = document.createElement("div");
		var list = document.createElement("ul");
		list.className = "CardSpotter";
		list.id = "TooltipList";
		tooltipDiv.appendChild(list);

		var historyDiv = document.createElement("div");
		historyDiv.className = "HistoryList";
		var history = document.createElement("ul");
		history.className = "HistoryList";
		history.id = "HistoryList";
		historyDiv.appendChild(history);
		tooltipDiv.appendChild(historyDiv);
	}
	tooltipDiv.id = "tooltipDiv";
	tooltipDiv.style.backgroundColor = "black";
	tooltipDiv.style.borderRadius = "7px";
	tooltipDiv.style.zIndex = "2147483647";

	tooltipDiv.width = cDetailsBaseWidth;
	tooltipDiv.clientWidth = cDetailsBaseWidth;
	tooltipDiv.style.maxWidth = cDetailsBaseWidth;
	tooltipDiv.style.position = "absolute";

	var mousegrabcanvas = document.getElementById("mousegrabcanvas");
	if (fullScreen || mousegrabcanvas == null) {
		var transformScale = gSettings.detailsTargetWidth / cDetailsBaseWidth;
		tooltipDiv.style.transform = "Scale(" + transformScale.toString() + ")";
		if (gSettings.horizontal == "left") {
			tooltipDiv.style.left = "0px";
			tooltipDiv.style.transformOrigin = "top left";
		}
		else {
			tooltipDiv.style.right = "0px";
			tooltipDiv.style.transformOrigin = "top right";
		}

		if (gSettings.vertical == "top") {
			var videoRect = toolTipParent.getBoundingClientRect();
			tooltipDiv.style.top = (videoRect.height * gSettings.clickareay).toString() + "px";
		}
		else {
			tooltipDiv.style.bottom = "0px";
		}

		if (tooltipDiv.parentNode == null || tooltipDiv.parentNode != toolTipParent.parentNode) {
			if (tooltipDiv.parentNode != null) {
				tooltipDiv.parentNode.removeChild(tooltipDiv);
			}
			toolTipParent.parentNode.insertBefore(tooltipDiv, toolTipParent.parentNode.firstChild);
		}
	}
	else //on the outside if possible
	{
		var transformScale = gSettings.detailsTargetWidth / cDetailsBaseWidth;
		tooltipDiv.style.transform = "Translate(" + gSettings.offsetx + "px," + gSettings.offsety + "px) Scale(" + transformScale.toString() + ")";
		var videoRect = getVideoClientRect();
		if (gSettings.mouseanchor) {
			tooltipDiv.style.left = (gx + cDetailsBaseWidth).toString() + "px";
			tooltipDiv.style.top = Math.max(videoRect.top + gy - cDetailsBaseHeight, 0).toString() + "px";
			tooltipDiv.style.transformOrigin = "top left";
		}
		else {
			if (gSettings.horizontal == "left") {
				tooltipDiv.style.left = Math.max(0, videoRect.left - tooltipDiv.width * transformScale).toString() + "px";
				tooltipDiv.style.transformOrigin = "top left";
			}
			else {
				tooltipDiv.style.left = Math.min(videoRect.right, document.body.clientWidth - tooltipDiv.width * transformScale).toString() + "px";
				tooltipDiv.style.transformOrigin = "top left";
			}

			if (gSettings.vertical == "top") {
				tooltipDiv.style.top = videoRect.top.toString() + "px";
			}
			else {
				tooltipDiv.style.top = Math.max(0, videoRect.bottom - cDetailsBaseHeight).toString() + "px";
			}
		}

		if (tooltipDiv.parentNode == null || tooltipDiv.parentNode == toolTipParent.parentNode) {
			if (tooltipDiv.parentNode != null) {
				tooltipDiv.parentNode.removeChild(tooltipDiv);
			}
			document.body.insertBefore(tooltipDiv, null);
		}
	}

	tooltipDiv.style.visibility = 'visible';
}

function ShowError(error) {
	ShowResult({
		id: 0,
		name: error,
		score: 0
	});
}

function downloadSearchImage(result) {
	var searchImage = document.getElementById("searchImage");
	if (searchImage != null) {
		var downloadLink = document.createElement("a");
		downloadLink.href = searchImage.src;
		downloadLink.download = "failed.png";
		if (name !== undefined) {
			downloadLink.download = name + ".png";
		}
		downloadLink.click();
	}
}

function ProcessResult(result) {
	lastresultstime = performance.now();

	if (result == undefined) {
		return;
	}

	if (gDebug)
	{
		console.log("CodeTime: " + result.time.toString());
	}

	if (gSettings.debugview && !result.isautomatch) {
		downloadSearchImage(result.name);
	}

	if (!result.success) {
		return;
	}

	ConvertToMouseCanvasSpace(result);
	ShowResult(result);
}

function ConvertToMouseCanvasSpace(result) {
	const xScale = GetClientToVideoScaleX();
	const yScale = GetClientToVideoScaleY();
	const videoToClientScale = (xScale > yScale) ? xScale : yScale;
	var rect = [{ x: result.px0, y: result.py0 }, { x: result.px1, y: result.py1 }, { x: result.px2, y: result.py2 }, { x: result.px3, y: result.py3 }];
	const inverseScale = toolTipParent.videoHeight / cExpectedVideoHeight;

	if (result.rescale == undefined)
		result.rescale = 1.0;

	for (var i = 0; i < 4; i++) {
		rect[i].x *= result.rescale; //internal rescale cardspotter side.. maybe that should already be compensated for?
		rect[i].x += result.pointx; //add roi start (to full input)
		rect[i].x *= inverseScale; //descale downsample (at scale)
		if (result.isautomatch) {
			rect[i].x += gSettings.automatchx * toolTipParent.videoWidth; //offset (now in video space)
		}
		rect[i].x -= gSettings.clickareax * toolTipParent.videoWidth; //offset (now in video canvas space)

		rect[i].x /= videoToClientScale; //videoToClient
		rect[i].y *= result.rescale;
		rect[i].y += result.pointy; //add roi start
		rect[i].y *= inverseScale; //descale downsample
		if (result.isautomatch) {
			rect[i].y += gSettings.automatchy * toolTipParent.videoHeight; //offset
		}
		rect[i].y -= gSettings.clickareay * toolTipParent.videoHeight; //offset (now in video canvas space)
		rect[i].y /= videoToClientScale; //videoToClient
	}
	result.px0 = rect[0].x;
	result.py0 = rect[0].y;
	result.px1 = rect[1].x;
	result.py1 = rect[1].y;
	result.px2 = rect[2].x;
	result.py2 = rect[2].y;
	result.px3 = rect[3].x;
	result.py3 = rect[3].y;
}

function handleErrorMessage(request, sender, sendResponse) {
	if (request.cmd == "novideo") {
		alert("CardSpotter - No HTML5 Video found.");
	}
}

function handlePopupOrBackgroundMessage(request, sender, sendResponse) {
//	console.log(request.cmd);

	if (request.cmd == "getmode") {
		if (gCurrentMode == "disabled") {
			sendResponse("disabled");
		}
		else {
			sendResponse(gSettings.cardpool);
		}
	}
	else if (request.cmd == "setmode") {
		SetMode(request.mode);
		chrome.runtime.sendMessage({ cmd: "modeset" });
	}
	else if (request.cmd == "log") {
		console.log(request.log);
	}
	else if (request.cmd == "crash") {
		SetMode("disabled");
		alert('CardSpotter has crashed - attempting automated extension reload.\nCardSpotter needs to be manually re-enabled.\nPlease report this to jonas@cardspotter.com');
	}
	else if (request.cmd == "showresults") //we always get results, both from clicksearch and auto
	{
		ProcessResults(request.results);
	}
}

function getVideos(videoTags) {
	let videos = [];
	for (var i = 0; i < videoTags.length; i++) {
		var video = videoTags.item(i);
		if (video.clientWidth > 250 && video.clientHeight > 150) {
			videos.push(video);
			break; //for now we ONLY do one video
		}
	}
	return videos;
};

var videos = getVideos(document.getElementsByTagName('video'));
var toolTipParent = (videos.length > 0 ) ? videos[0] : null;
var videoSearchers = [];

if (toolTipParent != null) {

	for (let i=0; i<videos.length; ++i)
	{
		videoSearchers.push(new VideoDetection(videos[i]));
	}

	var link = document.createElement('link');
	link.href = chrome.extension.getURL('') + 'content_script.css';
	link.rel = 'stylesheet';
	document.head.appendChild(link);

	var icons = document.createElement('link');
	icons.href = chrome.extension.getURL('') + 'css/materialdesignicons.min.css';
	icons.media = "all";
	icons.rel = 'stylesheet';
	document.head.appendChild(icons);

	UpdateSettings(function () {
		if (gSettings.resetmouseoffset) {
			gSettings.offsetx = 0;
			gSettings.offsety = 0;
			chrome.storage.sync.set({ offsety: 0, offsetx: 0 }, function () { });
		}
		chrome.runtime.onMessage.addListener(handlePopupOrBackgroundMessage);
		chrome.runtime.sendMessage({ cmd: "onload" });
	});
	
	chrome.storage.onChanged.addListener(function (changes, namespace) {
		UpdateSettings();
	});
}
else {
	chrome.runtime.onMessage.addListener(handleErrorMessage);
}

