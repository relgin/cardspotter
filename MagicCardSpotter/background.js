var cardSpotterLibSettings =[
"automatchhistorysize",
"cardpool",
"mincardsize",
"maxcardsize",
"okscore",
"goodscore"
];

var defaultSettings = {
	debugview: false,
	mousemode: "click",
	mousemodeenabled: true,
	mouseanchor : false,
	vertical: "top",
	offsety: 0,
	horizontal : "right",
	offsetx: 0,
	clickareaheight: 0.92,
	clickareawidth: 1.0,
	clickareax: 0,
	clickareay: 0.05,
	mincardsize: 7,
	maxcardsize: 15,
	goodscore: 84,
	okscore: 75,
	detailsTargetWidth:300,
	autoscreen:true,
	automatchtimeout:3000,
	automatchhistorysize:1,
	automatchwidth:0.7,
	automatchheight:1.0,
	automatchx:0.15,
	automatchy:0.0,
	autocolor:false,
	canvascolor:false,
	tooltiplogo:true,
	tooltipscryfall:true,
	showsavebutton:false,
	showhistory:true,
	resetmouseoffset:true,
	cardpool:""
	};
var settings = defaultSettings;


chrome.storage.onChanged.addListener(function(changes, namespace) {
	getSettings();
});

function setSetting(key, value)
{
	var stringKey = key.toString();
	var stringValue = value.toString();
	var array = stringToArrayBuffer(stringKey + stringValue);
	dataHeap.set(array);
	csSetSetting(dataHeap.byteOffset, stringKey.length, stringValue.length);
}

var myActiveTabId = -1;
var myCurrentMode = "disabled";

var lastScreen = null;
var gwidth = null;
var gheight = null;

function setCurrentTabMode(aMode)
{
	chrome.tabs.query({ active:true, currentWindow:true}, function(tabs)
	{
		setMode(tabs[0].id, aMode);
	});
}
function setEnabled() { setCurrentTabMode("enabled");}
function setDisabled() { setCurrentTabMode("disabled");}

function toggleEnabled(tab)
{
	if (myActiveTabId != tab.id || myCurrentMode == "disabled")
	{
		setMode(tab.id, "enabled");
	}
	else
	{
		setMode(tab.id, "disabled");
	}
}


function VideoFail()
{
	if (myActiveTabId!=-1)
	{
		chrome.tabs.sendMessage(myActiveTabId, {cmd: "novideo"});
		myActiveTabId = -1;
	}
}

function tabMessage(tabId, message)
{
	chrome.tabs.get(tabId,function() {
		if (chrome.runtime.lastError)
		{
			//console.log("Would have thrown");
		}
		else
		{
			chrome.tabs.sendMessage(tabId, message);
		}
	});
}

function setIcon()
{
	chrome.browserAction.setIcon({
	  path : { "16": "csLogo16.png",
		   "48": "csLogo48.png",
		  "128": "csLogo128.png" },
		  tabId : myActiveTabId
	});
	chrome.browserAction.setTitle({
	  title : "CardSpotter",
		  tabId : myActiveTabId
	});
}

var onloadTimeout;
function setMode(aTabId, aMode)
{
	if ((myActiveTabId == aTabId && aMode == "disabled" ) || (myActiveTabId != aTabId && aMode == "enabled"))
	{
		if (myActiveTabId!=-1)
		{
			tabMessage(myActiveTabId, {cmd: "setmode", mode :"disabled"});
			myActiveTabId = -1;
		}
	}
	
	if (aMode == "enabled")
	{
		myActiveTabId = aTabId;
		onloadTimeout = setTimeout(VideoFail, 1000);
		chrome.tabs.executeScript(myActiveTabId, {file: "content_script.js"});
	}
	
	myCurrentMode = aMode;
}

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab)
{
	if (tabId === myActiveTabId && changeInfo.status == 'loading')
	{
		setMode(tabId, "disabled");
	}
});

chrome.tabs.onRemoved.addListener(function(tabid, removed) {
	if (tabid == myActiveTabId)
	{
		setMode(tabid, "disabled");
	}
})

function _base64ToArrayBuffer(base64) {
    var binary_string =  window.atob(base64);
    var len = binary_string.length;
    var bytes = new Uint8Array( len );
    for (var i = 0; i < len; i++)        {
        bytes[i] = binary_string.charCodeAt(i);
    }
    return bytes;
}

function stringToArrayBuffer(string) {
    var len = string.length;
    var bytes = new Uint8Array( len );
    for (var i = 0; i < len; i++)        {
        bytes[i] = string.charCodeAt(i);
    }
    return bytes;
}

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
	if (request.cmd === "video")
	{
		addScreen(_base64ToArrayBuffer(request.videoData.substr(22)), request.width, request.height);
	}
	else if(request.cmd === "search")
	{
		findCard(_base64ToArrayBuffer(request.videoData.substr(22)), request.x, request.y, request.width, request.height);
	}
	else if(request.cmd === "onload")
	{
		clearTimeout(onloadTimeout);
		if (myActiveTabId != -1)
		{
			chrome.tabs.sendMessage(myActiveTabId, {cmd: "setmode", mode :myCurrentMode});
		}
	}
	else if(request.cmd === "defaultSettings")
	{
		chrome.storage.sync.set(defaultSettings);
	}
	else if(request.cmd === "getSettings")
	{
		sendResponse(settings);
	}
 });

var Module = {
print: (function() {
          return function(text) {
            if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
            console.log(text);
          };
        })(),
        printErr: function(text) {
          if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
          if (0) { // XXX disabled for safety typeof dump == 'function') {
            dump(text + '\n'); // fast, straight to the real console
          } else {
            console.error(text);
          }
        },
        canvas: (function() {
        })()
};

function setCardPool(array)
{
	dataHeap.set(array);
	csSetCardPool(dataHeap.byteOffset, array.length);
}


function addScreen(array, width, heigth)
{
	dataHeap.set(array);
	csAddScreen(dataHeap.byteOffset, array.length, width, heigth);
}

function findCard(array, px, py, width, heigth)
{
	dataHeap.set(array);
	csFindCard(dataHeap.byteOffset, array.length, width, heigth, px, py);
}

function loadDatabase(array)
{
	dataHeap.set(array);
	csLoadDatabase(dataHeap.byteOffset, array.length);
}

var width = 1280;
var height = 720;

var nDataBytes;
var dataPtr;
var dataHeap;


var script = document.createElement('script');
script.src = "cardspotter.js";
document.body.appendChild(script);

var csFindCard;
var csAddScreen;

function loadingDone()
{
	console.log("CardSpotter Loaded");
}

function load()
{
	csFindCard = Module.cwrap('FindCard', null, ['number', 'number', 'number','number', 'number', 'number']);
	csAddScreen = Module.cwrap('AddScreen', null, ['number', 'number', 'number', 'number']);
	csSetCardPool = Module.cwrap('SetCardPool', null, ['number', 'number']);
	csSetSetting = Module.cwrap('SetSetting', 'number', ['number', 'number', 'number']);
	csLoadDatabase = Module.cwrap('LoadDatabase', null, ['number', 'number']);
	
	chrome.browserAction.onClicked.addListener(function (tab){toggleEnabled(tab);});
	
	nDataBytes = width * height * 4;
	dataPtr = Module._malloc(nDataBytes);
	dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
	var extensionUrl = chrome.extension.getURL('');
	loadDatabase(stringToArrayBuffer(extensionUrl+"magic.db"));

	getSettings();
}

chrome.runtime.onInstalled.addListener(function (details)
{
	if (details.reason == "install")
	{
		chrome.tabs.create({url:'https://www.cardspotter.com/getstarted.html'});
		chrome.storage.sync.set(defaultSettings);
	}
	else if(details.reason == "update")
	{
		if (chrome.runtime.getManifest().version == "3.0.2")
		{
			chrome.storage.sync.set(defaultSettings);
		}
	}
});

function getSettings() {
	chrome.storage.sync.get(settings, function (items) {
		settings = items;
		for (var key in items) {
			if (cardSpotterLibSettings.indexOf(key) != -1) {
				setSetting(key, items[key]);
			}
		}
	});
}

