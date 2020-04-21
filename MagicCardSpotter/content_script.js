var gSettings = undefined;
var gCurrentMode = "disabled";
var cExpectedVideoHeight = 720;
var gDebug = false;

class VideoDetection {
	constructor(video, index) {
		this.video = video;
		this.index = index;
		this.autoSearchStartTime = performance.now();
		this.autoPostMessageTime = performance.now();
		this.videoCopy = document.createElement("canvas");
		this.searchWorker = new Worker(chrome.extension.getURL("worker.js"));
		this.mousegrabid = "mousegrabcanvas" + index.toString();
		this.lastVideoRect = null;

		this.gx = 0;
		this.gy = 0;

		this.lastGx = 0;
		this.lastGy = 0;

		let _this = this;
		this.searchWorker.onmessage = function (e) {

			if (e.data.result == undefined) {
				_this.ResetAutoSearch();
				_this.ResetSearch();
				return;
			}

			if (e.data.result.isautomatch) {
				_this.ResetAutoSearch();
			}

			if (!e.data.result.isautomatch) {
				_this.ResetSearch();
			}

			if (gDebug && !e.data.result.isautomatch) {
				let sendTime = _this.autoPostMessageTime - _this.autoSearchStartTime;
				let now = performance.now();
				let searchTime = now - _this.autoPostMessageTime;
				console.log("SendTime: " + sendTime.toString() + ", SearchTime: " + searchTime.toString());
			}

			e.data.result.vs = _this;
			_this.ConvertToMouseCanvasSpace(e.data.result);
			fProcessResult(e.data.result);
		}

		this.searchStartTime = performance.now();
		this.postMessageTime = performance.now();

		this.searchState = "done";
	}

	EnableMouseGrab() {
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		mousegrabcanvas.style.zIndex = cDivIndex;

	}

	DisableMouseGrab() {
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		mousegrabcanvas.style.zIndex = -1;
	}

	ClearSearchTimeout() {
		clearTimeout(this.gTriggerSearchTimeout);
	}

	Setup() {

		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		if (mousegrabcanvas == null) {
			mousegrabcanvas = document.createElement("canvas");
			mousegrabcanvas.id = this.mousegrabid;
			mousegrabcanvas.style.position = "absolute";
			mousegrabcanvas.style.top = "0px";
			mousegrabcanvas.style.left = "0px";
			this.InsertMouseGrabCanvas(mousegrabcanvas);
		}
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

		let areaWidth = gSettings.automatchwidth;
		let areaHeight = gSettings.automatchheight;
		let xStart = gSettings.automatchx;
		let yStart = gSettings.automatchy;
		let scale = cExpectedVideoHeight / this.video.videoHeight;
		this.videoCopy.height = this.video.videoHeight * scale * areaHeight;
		this.videoCopy.width = this.video.videoWidth * scale * areaWidth;

		let copyContext = this.videoCopy.getContext('2d');
		copyContext.clearRect(0, 0, this.videoCopy.width, this.videoCopy.height);
		copyContext.drawImage(this.video, this.video.videoWidth * xStart, this.video.videoHeight * yStart, this.video.videoWidth * areaWidth, this.video.videoHeight * areaHeight, 0, 0, this.videoCopy.width, this.videoCopy.height);

		const imageData = copyContext.getImageData(0, 0, this.videoCopy.width, this.videoCopy.height);
		const uintArray = imageData.data;
		this.searchWorker.postMessage({ action: 'ADD_SCREEN', width: this.videoCopy.width, height: this.videoCopy.height, imageData: uintArray }, [uintArray.buffer]);

		this.autoPostMessageTime = performance.now();
	}

	Search(mx, my) {
		this.searchStartTime = performance.now();

		let { rx, ry } = this.MouseToVideoRelativeCoords(mx, my);

		let sx = rx * this.video.videoWidth;
		let sy = ry * this.video.videoHeight;

		this.searchState = "search";
		document.body.style.cursor = "wait";

		let cTimeoutValue = 5000;

		this.searchTimeout = setTimeout(this.ResetSearch, 5000);

		fImageSearchVideoCopy(this.videoCopy, this.video, sx, sy);

		const imageData = this.videoCopy.getContext('2d').getImageData(0, 0, this.videoCopy.width, this.videoCopy.height);
		const uintArray = imageData.data;
		this.searchWorker.postMessage({ action: 'FIND_CARD', width: this.videoCopy.width, height: this.videoCopy.height, imageData: uintArray, x: sx, y: sy }, [uintArray.buffer]);

		fShowTooltipSearchImage(sx, sy);

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
		this.screenTimeout = setTimeout(function () { _this.PostScreenshot(); }, 250);
	}

	SetSetting(k, v) {
		this.searchWorker.postMessage({ action: 'SET_SETTING', key: k, value: v });
	}

	UpdateDebugColors() {
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		if (mousegrabcanvas != null) {
			if (gSettings.canvascolor) {
				mousegrabcanvas.style.backgroundColor = "rgba(122,255,122,0.5)";
			}
			else {
				mousegrabcanvas.style.backgroundColor = "rgba(0,0,0,0)";
			}
		}

		let autocolor = document.getElementById("autocolor");
		let showColor = gSettings.autocolor && !this.IsFullScreen();
		if (showColor) {
			if (autocolor == null) {
				autocolor = document.createElement("canvas");
				autocolor.style.backgroundColor = "rgba(122,122,255,0.5)";
				autocolor.id = "autocolor";
				autocolor.style.zIndex = cMaxIndex;
				autocolor.style.position = "absolute";
				document.body.insertBefore(autocolor, document.body.firstChild);
			}

			let videoRect = this.GetVideoClientRect();
			let areaWidth = gSettings.automatchwidth * videoRect.width;
			let areaHeight = gSettings.automatchheight * videoRect.height;
			let xStart = gSettings.automatchx * videoRect.width;
			let yStart = gSettings.automatchy * videoRect.height;
			autocolor.style.top = (videoRect.top + yStart).toString() + "px";
			autocolor.style.left = (videoRect.left + xStart).toString() + "px";
			autocolor.width = areaWidth;
			autocolor.height = areaHeight;
		}
		else if (autocolor != null) {
			autocolor.parentNode.removeChild(autocolor);
		}
	}

	fadeOutRectangle(rect) {
		let steps = 15;
		let fadesteps = 10;
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		let manualContext = mousegrabcanvas.getContext("2d");
		manualContext.clearRect(0, 0, mousegrabcanvas.width, mousegrabcanvas.height);
		clearInterval(this.interval);

		let _this = this;
		this.i = 0;
		this.interval = setInterval(function () {
			let color = 'rgba(50,50,255,' + ((fadesteps - Math.max(_this.i + fadesteps - steps, 0)) / fadesteps - 1 / fadesteps) + ')';
			_this.drawRect(rect, color);
			_this.i++;
			if (_this.i === steps) { // stop if done
				let mousegrabcanvas = document.getElementById(_this.mousegrabid);
				let manualContext = mousegrabcanvas.getContext("2d");

				manualContext.clearRect(0, 0, mousegrabcanvas.width, mousegrabcanvas.height);
				clearInterval(_this.interval);
			}
		}, 100);
	}


	UpdateCanvasSize() {
		let videoRect = this.GetClientRect();

		if (this.lastVideoRect == null || this.lastVideoRect.top != videoRect.top || this.lastVideoRect.left != videoRect.left || this.lastVideoRect.width != videoRect.width || this.lastVideoRect.height != videoRect.height) {
			this.lastVideoRect = videoRect;

			let nowFullscreen = this.IsFullScreen();

			if (this.fullScreen != nowFullscreen) {
				this.fullScreen = nowFullscreen;

				let mousegrabcanvas = document.getElementById(this.mousegrabid);
				if (mousegrabcanvas.parentNode != undefined) {
					mousegrabcanvas.parentNode.removeChild(mousegrabcanvas);
				}

				this.InsertMouseGrabCanvas(mousegrabcanvas);
			}

			this.UpdateMouseCanvasSize();
		}
	}

	UpdateMouseCanvasSize() {
		let videoRect = this.GetClientRect();
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
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
		mousegrabcanvas.style.zIndex = cDivIndex;
	}

	InsertMouseGrabCanvas(mousegrabcanvas) {
		if (this.IsFullScreen()) {
			this.video.parentNode.insertBefore(mousegrabcanvas, this.video.parentNode.firstChild);
		}
		else {
			document.body.insertBefore(mousegrabcanvas, document.body.firstChild);
		}
	}

	MouseToVideoRelativeCoords(mx, my) {
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		let canvasRect = mousegrabcanvas.getBoundingClientRect();
		let rx = mx / canvasRect.width;
		let ry = my / canvasRect.height;
		rx *= (gSettings.clickareawidth - gSettings.clickareax);
		rx += gSettings.clickareax;
		ry *= (gSettings.clickareaheight - gSettings.clickareay);
		ry += gSettings.clickareay;
		return { rx, ry };
	}

	IsFullScreen() {
		return this.video.scrollWidth > screen.width - 30;
	}

	GetClientRect() {
		return fGetVideoClientRect(this.video);
	}

	OnMouseStopped() {
		this.lastGx = this.gx;
		this.lastGy = this.gy;
		Search(this.gx, this.gy);
	}

	UseMouseOver() {
		if (this.fullScreen || gSettings.mousemode == "mouseover") {
			let tooltipDiv = document.getElementById("tooltipDiv");
			if (tooltipDiv != null && tooltipDiv.style.visibility == 'visible') {
				let tooltipRect = tooltipDiv.getBoundingClientRect();
				let mousegrabcanvas = document.getElementById(this.mousegrabid);
				let canvasRect = mousegrabcanvas.getBoundingClientRect();

				let mx = gx + canvasRect.left;
				let my = gy + canvasRect.top;
				if (tooltipRect.left < mx && tooltipRect.right > mx && tooltipRect.top < my && tooltipRect.bottom > my) {
					return false;
				}
			}
			return true;
		}
		return false;
	}

	UseMouseClick() {
		return !this.fullScreen && gSettings.mousemode == "click";
	}

	OnMouseMove(e) {
		this.gx = e.offsetX;
		this.gy = e.offsetY;

		clearTimeout(this.gTriggerSearchTimeout);

		let dx = this.lastGx - this.gx;
		let dy = this.lastGy - this.gy;

		if (this.UseMouseOver()) {
			if ((dx * dx + dy * dy) > (15 * 15)) {
				let _this = this;
				this.gTriggerSearchTimeout = setTimeout(function () { _this.OnMouseStopped }, 50);
			}
		}
	}

	OnMouseDown(e) {
		if (this.UseMouseClick()) {
			this.gx = e.offsetX;
			this.gy = e.offsetY;
			this.lastGx = this.gx;
			this.lastGy = this.gy;
			this.Search(this.gx, this.gy);
		}
	}

	OnMouseOver() {
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		let _this = this;
		mousegrabcanvas.onmouseover = null;
		mousegrabcanvas.onmousemove = function (e) { _this.OnMouseMove(e) };
		mousegrabcanvas.onmousedown = function (e) { _this.OnMouseDown(e) };
		mousegrabcanvas.onmouseout = function () { _this.OnMouseOut() };
		this.UpdateMouseCanvasSize();
	}

	OnMouseOut() {
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		let _this = this;
		mousegrabcanvas.onmouseover = function (e) { _this.OnMouseOver(e) };
		mousegrabcanvas.onmousemove = null;
		mousegrabcanvas.onmousedown = null;
		mousegrabcanvas.onmouseout = null;
		this.UpdateMouseCanvasSize();
		clearTimeout(this.gTriggerSearchTimeout);
	}

	AddMouseOver() {
		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		let _this = this;
		mousegrabcanvas.onmouseover = function (e) { _this.OnMouseOver(e) };
	}

	ConvertToMouseCanvasSpace(result) {
		const xScale = fGetClientToVideoScaleX(this.video);
		const yScale = fGetClientToVideoScaleY(this.video);
		const videoToClientScale = (xScale > yScale) ? xScale : yScale;
		let rect = [{ x: result.px0, y: result.py0 }, { x: result.px1, y: result.py1 }, { x: result.px2, y: result.py2 }, { x: result.px3, y: result.py3 }];
		const inverseScale = this.video.videoHeight / cExpectedVideoHeight;

		if (result.rescale == undefined)
			result.rescale = 1.0;

		for (let i = 0; i < 4; i++) {
			rect[i].x *= result.rescale; //internal rescale cardspotter side.. maybe that should already be compensated for?
			rect[i].x += result.pointx; //add roi start (to full input)
			rect[i].x *= inverseScale; //descale downsample (at scale)
			if (result.isautomatch) {
				rect[i].x += gSettings.automatchx * this.video.videoWidth; //offset (now in video space)
			}
			rect[i].x -= gSettings.clickareax * this.video.videoWidth; //offset (now in video canvas space)

			rect[i].x /= videoToClientScale; //videoToClient
			rect[i].y *= result.rescale;
			rect[i].y += result.pointy; //add roi start
			rect[i].y *= inverseScale; //descale downsample
			if (result.isautomatch) {
				rect[i].y += gSettings.automatchy * this.video.videoHeight; //offset
			}
			rect[i].y -= gSettings.clickareay * this.video.videoHeight; //offset (now in video canvas space)
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

	drawRect(rect, color) {
		let box = {};
		box.minx = Math.min(rect[0].x, rect[1].x, rect[2].x, rect[3].x);
		box.maxx = Math.max(rect[0].x, rect[1].x, rect[2].x, rect[3].x);
		box.miny = Math.min(rect[0].y, rect[1].y, rect[2].y, rect[3].y);
		box.maxy = Math.max(rect[0].y, rect[1].y, rect[2].y, rect[3].y);

		let mousegrabcanvas = document.getElementById(this.mousegrabid);
		let manualContext = mousegrabcanvas.getContext("2d");
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
}

var gUpdateTimeout;

var cUnblockSettingsWidth = 200;//Change this to be max cap based on client size (not fixed offset)

var cDetailsBaseWidth = 224;
var cDetailsBaseHeight = 324;
var cDetailsImgHeight = 308;

var resultHistory = [];

var lastresultstime = performance.now();

const cMaxIndex = "2147483647";
const cDivIndex = "2147483646";

var extensionUrl = chrome.extension.getURL('');

var cardSpotterLibSettings = [
	"automatchhistorysize",
	"cardpool",
	"mincardsize",
	"maxcardsize",
	"okscore",
	"goodscore"
];


function fUpdateMouseSearchHighlight() {
	let mini = document.getElementById("mini");
	if (mini != null) {
		if (!gSettings.mousemodeenabled) {

			videoSearchers.forEach(function (vs) {
				vs.DisableMouseGrab();
			});


			mini.classList.remove('mdi-light');
		}
		else {

			videoSearchers.forEach(function (vs) {
				vs.EnableMouseGrab();
			});

			mini.classList.add('mdi-light');
		}
	}

}

function fUpdateSettings(callback) {
	let wasAutoscreen = gSettings != undefined && gSettings.autoscreen != undefined && gSettings.autoscreen; //misses if we enable/disable on the same video
	chrome.storage.sync.get(null, function (items) {
		gSettings = items;
		fUpdateAutoScreenHighlight();
		fUpdateMouseSearchHighlight();
		if (gSettings.autoscreen && !wasAutoscreen) {

			videoSearchers.forEach(function (vs) {
				vs.PostScreenshot();
			});
		}

		for (let key in items) {
			if (cardSpotterLibSettings.indexOf(key) != -1) {

				videoSearchers.forEach(function (vs) {
					vs.SetSetting(key, items[key].toString());
				});
			}
		}

		videoSearchers.forEach(function (vs) {
			vs.UpdateDebugColors();
		});

		if (callback !== undefined) {
			callback();
		}
	});
}



function fSaveHistory() {
	var miniHistory = [];
	for (let i = 0; i < resultHistory.length; i++) {
		let result = resultHistory[i];
		miniHistory.push({ name: result.name, set: result.setcode, price: result.price });
	}

	let _myArray = JSON.stringify(miniHistory, null, 4); //indentation in json format, human readable

	let vLink = document.createElement('a'),
		vBlob = new Blob([_myArray], { type: "octet/stream" }),
		vName = 'matchHistory.json',
		vUrl = window.URL.createObjectURL(vBlob);
	vLink.setAttribute('href', vUrl);
	vLink.setAttribute('download', vName);
	vLink.click();
}

function fUpdate() {
	if (gCurrentMode == "disabled") {
		return;
	}

	videoSearchers.forEach(function (vs) {
		vs.UpdateCanvasSize();
	});

	fUpdateSettings(function () { gUpdateTimeout = setTimeout(fUpdate, 500); });
}


function fSetPixel(imageData, x, y, r, g, b, a) {
	index = (x + y * imageData.width) * 4;
	imageData.data[index + 0] = r;
	imageData.data[index + 1] = g;
	imageData.data[index + 2] = b;
	imageData.data[index + 3] = a;
}

function fClearTooltipDiv() {
	let tooltipDiv = document.getElementById("tooltipDiv");
	if (tooltipDiv != null) {
		while (tooltipDiv.firstChild) {
			tooltipDiv.removeChild(tooltipDiv.firstChild);
		}
	}
}

function fShowTooltipSearchImage(sx, sy) {
	let tooltipImage = document.getElementById("TooltipImage");
	if (tooltipImage != null) {
		let debugImage = document.getElementById("debugImage");
		if (debugImage == null) {
			debugImage = document.createElement("canvas");
			debugImage.id = "debugImage";
			debugImage.height = tooltipImage.height;
			debugImage.width = tooltipImage.width - 16;
		}
		if (debugImage != null) {
			fImageSearchVideoCopy(debugImage, firstVideo, sx, sy);
			let searchImage = tooltipImage.cloneNode(false);
			searchImage.id = "searchImage";
			searchImage.src = debugImage.toDataURL("image/png");
			tooltipImage.parentNode.appendChild(searchImage);
			tooltipImage.parentNode.removeChild(tooltipImage);
		}
	}
}


function fClearState() {
	videoSearchers.forEach(function (vs) {
		vs.ClearSearchTimeout();
	});
	document.body.style.cursor = "auto";
	gSearchState = "done";
}

function fLength(vec) {
	return Math.sqrt(vec.x * vec.x + vec.y * vec.y);
}

function fRotateVector(vec, deg) {
	let rad = deg * (Math.PI / 180)
	return { x: vec.x * Math.cos(rad) - vec.y * Math.sin(rad), y: vec.x * Math.sin(rad) + vec.y * Math.cos(rad) };
}

function fImageSearchVideoCopy(canvas, video, px, py) {
	let scale = cExpectedVideoHeight / video.videoHeight;
	canvas.height = scale * video.videoHeight * Math.min(1.0, (gSettings.maxcardsize / 100.0) * 2.0);
	canvas.width = canvas.height;

	let copyContext = canvas.getContext('2d');
	copyContext.clearRect(0, 0, canvas.width, canvas.height);
	copyContext.drawImage(video, px - canvas.width * 0.5 / scale, py - canvas.height * 0.5 / scale, canvas.width / scale, canvas.height / scale, 0, 0, canvas.width, canvas.height);
}

function fUpdateAutoScreenHighlight() {
	let csrenew = document.getElementById("csrenew");
	if (csrenew != null) {
		if (!gSettings.autoscreen) {
			csrenew.classList.remove('mdi-light');
		}
		else {
			csrenew.classList.add('mdi-light');
		}
	}
}

function fCreateMenu() {
	let namediv = document.getElementById("namediv");

	let bottom = document.createElement("div");
	bottom.classList.add("CardSpotterBottom");
	namediv.appendChild(bottom);

	let menuDiv = document.createElement("div");
	menuDiv.id = "CardSpotterMenu";
	menuDiv.classList.add("CardSpotterMenu");
	bottom.appendChild(menuDiv);

	let mini = document.createElement("i");
	mini.id = "mini";
	mini.classList.add("mdi", "mdi-mouse", "mdi-18px");

	fUpdateMouseSearchHighlight();

	mini.onclick = function () {
		chrome.storage.sync.set(
			{
				mousemodeenabled: !gSettings.mousemodeenabled

			}, fUpdateMouseSearchHighlight);
	}
	menuDiv.appendChild(mini);

	let csrenew = document.createElement("i");
	csrenew.id = "csrenew";
	csrenew.classList.add("mdi", "mdi-autorenew", "mdi-18px");
	csrenew.title = "Toggle automatic tooltip";
	fUpdateAutoScreenHighlight();

	csrenew.onclick = function () {
		if (!gSettings.autoscreen)
			PostScreenshot();

		chrome.storage.sync.set(
			{
				autoscreen: !gSettings.autoscreen

			}, fUpdateAutoScreenHighlight);
	}

	menuDiv.appendChild(csrenew);

	let cssettings = document.createElement("i");
	cssettings.id = "cssettings";
	cssettings.title = "Options";
	cssettings.classList.add("mdi", "mdi-settings", "mdi-18px", "mdi-flip-horizontal");
	cssettings.onclick = function () { window.open(extensionUrl + "options.html"); }
	menuDiv.appendChild(cssettings);


	if (gSettings.showsavebutton) {
		let cssave = document.createElement("i");
		cssave.id = "cssave";
		cssave.title = "Save match history to file.";
		cssave.classList.add("mdi", "mdi-content-save", "mdi-18px", "mdi-flip-horizontal");
		cssave.onclick = function () { fSaveHistory(); }
		menuDiv.appendChild(cssave);
	}
}

function fSetup() {

	videoSearchers.forEach(function (vs) {
		vs.Setup();
	});

	let namediv = document.getElementById("namediv");
	if (namediv == null) {
		namediv = document.createElement("div");
		namediv.id = "namediv";

		if (firstVideo != null)//fullScreen)
		{
			firstVideo.parentNode.insertBefore(namediv, firstVideo.parentNode.firstChild);
		}
		else {
			document.body.insertBefore(namediv, document.body.firstChild);
		}
		fCreateMenu();

		let menudiv = document.getElementById("CardSpotterMenu");
		menudiv.className = "CardSpotterMenu CardSpotterMenuBig";
	}
}

function fTeardown() {
	clearTimeout(gUpdateTimeout);
	//	chrome.runtime.onMessage.removeListener(handlePopupOrBackgroundMessage);
	let namediv = document.getElementById("namediv");
	if (namediv != null) {
		namediv.parentNode.removeChild(namediv);
	}
	let mousegrabcanvas = document.getElementById(this.mousegrabid);
	if (mousegrabcanvas != null) {
		mousegrabcanvas.parentNode.removeChild(mousegrabcanvas);
	}
	let tooltipDiv = document.getElementById("tooltipDiv");
	if (tooltipDiv != null) {
		tooltipDiv.parentNode.removeChild(tooltipDiv);
	}
}

function fSetMode(aMode) {

	if (gCurrentMode == aMode) {
		return;
	}

	fClearState();

	if (aMode == "disabled") {
		gCurrentMode = aMode;
		fTeardown();
		return;
	}

	if (gCurrentMode == "disabled") {
		fSetup();
		gCurrentMode = aMode;
		fUpdate();
	}

	gCurrentMode = aMode;

	videoSearchers.forEach(function (vs) {
		vs.AddMouseOver();
	});

}

function fSetCanvasImage(inputCanvas, request) {
	let view = new Uint8Array(request.imgdata);

	let height = request.height;
	let width = request.width;
	let channels = request.channels;

	inputCanvas.width = width;
	inputCanvas.height = height;

	let context = inputCanvas.getContext("2d");
	context.fillStyle = "green";
	context.fillRect(0, 0, width, height);

	let imageData = context.createImageData(width, height);
	for (y = 0; y < height; y++) {
		let rowStart = y * width * channels;
		for (x = 0; x < width; x++) {
			let b = view[rowStart + x * channels + 0];
			let g = view[rowStart + x * channels + 1];
			let r = view[rowStart + x * channels + 2];
			if (r == undefined || g == undefined || b == undefined) {
				continue;
			}
			fSetPixel(imageData, x, y, r, g, b, 255); // 255 opaque
		}
	}
	context.putImageData(imageData, 0, 0); // at coords 0,0
}

var imageCache = {};

function fGetImage(multiverseid, imageurl, usename, cardName, callback) {
	let img = imageCache[imageurl];
	if (img == undefined || usename) {
		img = new Image();
		img.src = imageurl;
		img.width = 224;
		img.height = 313;
		imageCache[imageurl] = img;
	}

	callback(img);
}

function fTooltipAddText(list, text) {
	let entry = document.createElement("li");
	entry.className = "CardSpotter";
	entry.appendChild(document.createTextNode(text));
	list.appendChild(entry);
	return entry;
}

function fTooltipAddLink(list, url, title, text) {
	let entry = document.createElement("li");
	entry.className = "CardSpotter";
	let link = document.createElement("a");
	link.className = "CardSpotter";
	link.href = url;
	link.target = "_blank";
	link.title = title;
	link.appendChild(document.createTextNode(text));
	entry.appendChild(link);
	list.appendChild(entry);
}

function fTooltipCreateBigImageLi() {
	let list = document.getElementById("TooltipList");
	let entry = document.createElement("li");
	entry.className = "CardSpotter";
	entry.style.height = cDetailsImgHeight.toString() + "px";
	list.appendChild(entry);
	return entry;
}

function fTooltipAddQualityItem(list, scoreNumber) {
	let confidence = Math.round(100 - ((100 * scoreNumber) / 1024));
	if (confidence > 98) {
		return;
	}

	let entry = document.createElement("li");
	entry.className = "CardSpotter";
	entry.innerText = confidence.toString() + "%";
	list.appendChild(entry);

	return entry;
}



var interval;

function fBlinkCardRect(result) {
	let rect = [{ x: result.px0, y: result.py0 }, { x: result.px1, y: result.py1 }, { x: result.px2, y: result.py2 }, { x: result.px3, y: result.py3 }];
	result.vs.fadeOutRectangle(rect);
}

function fGetImageUrl(coreUrl) {
	let cUseLocalImages = false;
	if (cUseLocalImages) {
		return extensionUrl + "images/" + coreUrl + ".png";
	}
	let useFast = true;
	if (useFast) {
		return "https://img.scryfall.com/cards/normal/" + coreUrl + ".jpg";
	}
	return "https://img.scryfall.com/cards/png/" + coreUrl + ".png";
}

function fShowResult(result) {
	fCreateTooltipDiv();

	let id = result.id;
	let cardName = result.name;
	let setcode = result.setcode;
	let imageurl = fGetImageUrl(result.url);
	let score = result.score;

	let tooltipDiv = document.getElementById("tooltipDiv");
	if (result.isautomatch) {
		fBlinkCardRect(result);
	}

	if (gSettings.tooltiplogo) {
		fCreateNameBar(tooltipDiv);
	}

	let imgEntry = fTooltipCreateBigImageLi();

	if (id != undefined) {
		let usename = id < 0;
		fGetImage(id, imageurl, usename, cardName, function (img) {
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
		fTooltipAddText(document.getElementById("TooltipList"), cardName);
		return;
	}

	if (gSettings.debugview) {
		let path = (result.path == undefined) ? "?" : result.path;
		let sendTime = (result.isautomatch) ? result.vs.autoPostMessageTime - result.vs.autoSearchStartTime : result.vs.postMessageTime - result.vs.searchStartTime;
		let now = performance.now();
		let searchTime = now - ((result.isautomatch) ? result.vs.autoPostMessageTime : result.vs.postMessageTime);
		fTooltipAddText(document.getElementById("TooltipList"), "SendTime: " + sendTime.toString() + ", SearchTime: " + searchTime.toString() + ", Score: " + score.toString() + ", Path: " + path.toString());
	}

	if (gSettings.tooltipscryfall) {
		let tooltipList = document.getElementById("TooltipList");
		let li = document.createElement("li");
		let linkList = document.createElement("ul");
		li.appendChild(linkList);
		linkList.className = "CardSpotter";
		linkList.id = "LinkList";
		tooltipList.appendChild(li);

		fTooltipAddQualityItem(linkList, score);
		fTooltipAddLink(linkList, "https://scryfall.com/card/" + encodeURIComponent(setcode) + "/" + id, "Scryfall", "Scryfall");
	}

	tooltipDiv.onmouseover = function () {
		fBlinkCardRect(result);
		tooltipDiv.onmouseover = null; //TODO: this prevents flicker
		let historyList = document.getElementById("HistoryList");
		while (historyList.firstChild) {
			historyList.removeChild(historyList.firstChild);
		}

		if (gSettings.showhistory) {
			fPopulateHistoryList(historyList);
		}
	}
}

function fPopulateHistoryList(historyList) {
	let row;
	let c = 0;
	function showHistoricalResult(h) { return function () { let r2 = resultHistory[h]; fShowResult(r2); }; }
	;
	for (let i = Math.max(0, resultHistory.length - 10); i < resultHistory.length; i++) {
		if (c % 5 == 0) {
			row = document.createElement("li");
			historyList.appendChild(row);
		}
		let r = resultHistory[i];
		let img = imageCache[fGetImageUrl(r.url)];
		if (img != undefined) {
			let historyImg = img.cloneNode(false);
			historyImg.className = "HistoryList";
			historyImg.onmousedown = showHistoricalResult(i);
			row.appendChild(historyImg);
			c += 1;
		}
	}
	historyList.style.visibility = 'visible';
}

function fCreateNameBar(tooltipDiv) {
	let list = document.getElementById("TooltipList");
	let entry = document.createElement("li");
	entry.className = "CardSpotter Move";
	list.appendChild(entry);
	let img = new Image();
	img.src = extensionUrl + "cardspottername.png";
	img.className = "CardSpotter";
	img.id = "CardSpotterName";
	entry.appendChild(img);
	entry.onmousedown = function (ed) {
		let cachedOffsetX = gSettings.offsetx;
		let cachedOffsetY = gSettings.offsety;
		document.onmousemove = function (e) {
			let dx = e.movementX;
			let dy = e.movementY;
			cachedOffsetX += dx;
			cachedOffsetY += dy;
			let transformScale = gSettings.detailsTargetWidth / cDetailsBaseWidth;
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

function fGetClientToVideoScaleX(video) {
	return video.videoWidth / video.clientWidth;
}
function fGetClientToVideoScaleY(video) {
	return video.videoHeight / video.clientHeight;
}
function fGetVideoClientRect(video) {
	let rect = video.getBoundingClientRect();
	const xScale = fGetClientToVideoScaleX(video);
	const yScale = fGetClientToVideoScaleY(video);
	if (xScale < yScale) //div is wider than high
	{
		const actualWidth = video.videoWidth / yScale;//even scale on video so it will use the larger one
		const diff = rect.width - actualWidth;
		rect.x = rect.x + (diff / 2.0);
		rect.width = actualWidth;
	}
	return rect;
}

function fCreateTooltipDiv() {
	let tooltipDiv = document.getElementById("tooltipDiv");
	if (tooltipDiv != null)//insanity
	{
		tooltipDiv.parentNode.removeChild(tooltipDiv);
		tooltipDiv = null;
	}

	if (tooltipDiv == null) {
		tooltipDiv = document.createElement("div");
		let list = document.createElement("ul");
		list.className = "CardSpotter";
		list.id = "TooltipList";
		tooltipDiv.appendChild(list);

		let historyDiv = document.createElement("div");
		historyDiv.className = "HistoryList";
		let history = document.createElement("ul");
		history.className = "HistoryList";
		history.id = "HistoryList";
		historyDiv.appendChild(history);
		tooltipDiv.appendChild(historyDiv);
	}
	tooltipDiv.id = "tooltipDiv";
	tooltipDiv.style.backgroundColor = "black";
	tooltipDiv.style.borderRadius = "7px";
	tooltipDiv.style.zIndex = cMaxIndex;

	tooltipDiv.width = cDetailsBaseWidth;
	tooltipDiv.clientWidth = cDetailsBaseWidth;
	tooltipDiv.style.maxWidth = cDetailsBaseWidth;
	tooltipDiv.style.position = "absolute";

	let fullScreen = false;

	videoSearchers.forEach(function (vs) {
		fullScreen = fullScreen || vs.fullScreen;
	});

	if (fullScreen) {
		let transformScale = gSettings.detailsTargetWidth / cDetailsBaseWidth;
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
			let videoRect = firstVideo.getBoundingClientRect();
			tooltipDiv.style.top = (videoRect.height * gSettings.clickareay).toString() + "px";
		}
		else {
			tooltipDiv.style.bottom = "0px";
		}

		if (tooltipDiv.parentNode == null || tooltipDiv.parentNode != firstVideo.parentNode) {
			if (tooltipDiv.parentNode != null) {
				tooltipDiv.parentNode.removeChild(tooltipDiv);
			}
			firstVideo.parentNode.insertBefore(tooltipDiv, firstVideo.parentNode.firstChild);
		}
	}
	else //on the outside if possible
	{
		let transformScale = gSettings.detailsTargetWidth / cDetailsBaseWidth;
		tooltipDiv.style.transform = "Translate(" + gSettings.offsetx + "px," + gSettings.offsety + "px) Scale(" + transformScale.toString() + ")";
		let videoRect = fGetVideoClientRect(firstVideo);
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

		if (tooltipDiv.parentNode == null || tooltipDiv.parentNode == firstVideo.parentNode) {
			if (tooltipDiv.parentNode != null) {
				tooltipDiv.parentNode.removeChild(tooltipDiv);
			}
			document.body.insertBefore(tooltipDiv, null);
		}
	}

	tooltipDiv.style.visibility = 'visible';
}

function fShowError(error) {
	fShowResult({
		id: 0,
		name: error,
		score: 0
	});
}

function fDownloadSearchImage(result) {
	let searchImage = document.getElementById("searchImage");
	if (searchImage != null) {
		let downloadLink = document.createElement("a");
		downloadLink.href = searchImage.src;
		downloadLink.download = "failed.png";
		if (name !== undefined) {
			downloadLink.download = name + ".png";
		}
		downloadLink.click();
	}
}

function fProcessResult(result) {
	lastresultstime = performance.now();

	if (result == undefined) {
		return;
	}

	if (gDebug) {
		console.log("CodeTime: " + result.time.toString());
	}

	if (gSettings.debugview && !result.isautomatch) {
		fDownloadSearchImage(result.name);
	}

	if (!result.success) {
		return;
	}

	fShowResult(result);
}



function fHandleErrorMessage(request, sender, sendResponse) {
	if (request.cmd == "novideo") {
		alert("CardSpotter - No HTML5 Video found.");
	}
}

function fHandlePopupOrBackgroundMessage(request, sender, sendResponse) {
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
		fSetMode(request.mode);
		chrome.runtime.sendMessage({ cmd: "modeset" });
	}
	else if (request.cmd == "log") {
		console.log(request.log);
	}
	else if (request.cmd == "crash") {
		fSetMode("disabled");
		alert('CardSpotter has crashed - attempting automated extension reload.\nCardSpotter needs to be manually re-enabled.\nPlease report this to jonas@cardspotter.com');
	}
	else if (request.cmd == "showresults") //we always get results, both from clicksearch and auto
	{
		ProcessResults(request.results);
	}
}

function fGetVideos(videoTags) {
	let videos = [];
	for (let i = 0; i < videoTags.length; i++) {
		let video = videoTags.item(i);
		if (video.clientWidth > 250 && video.clientHeight > 150) {
			videos.push(video);
			if (!gSettings.multivideo)
			{
				break;
			}
		}
	}
	return videos;
};

let videos = [];
let firstVideo = null;
let videoSearchers = [];

chrome.runtime.onMessage.addListener(fHandlePopupOrBackgroundMessage);
chrome.runtime.onMessage.addListener(fHandleErrorMessage);

fUpdateSettings(function () {
	if (gSettings.resetmouseoffset) {
		gSettings.offsetx = 0;
		gSettings.offsety = 0;
		chrome.storage.sync.set({ offsety: 0, offsetx: 0 }, function () { });
	}

	videos = fGetVideos(document.getElementsByTagName('video'));
	if (videos.length>0)
	{
		firstVideo = videos[0];
		for (let i = 0; i < videos.length; ++i) {
			videoSearchers.push(new VideoDetection(videos[i], i));
		}
	
		let link = document.createElement('link');
		link.href = chrome.extension.getURL('') + 'content_script.css';
		link.rel = 'stylesheet';
		document.head.appendChild(link);
	
		let icons = document.createElement('link');
		icons.href = chrome.extension.getURL('') + 'css/materialdesignicons.min.css';
		icons.media = "all";
		icons.rel = 'stylesheet';
		document.head.appendChild(icons);
	
		chrome.runtime.sendMessage({ cmd: "onload" });
		chrome.storage.onChanged.addListener(function (changes, namespace) {
			fUpdateSettings();
		});
	}
});

