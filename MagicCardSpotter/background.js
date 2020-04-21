
var defaultSettings = {
	debugview: false,
	multivideo : false,
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
	automatchhistorysize:3,
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

var myActiveTabId = -1;
let myTabHistory = [];
var myCurrentMode = "disabled";

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
var modeTimeout;

function inject()
{
	onloadTimeout = setTimeout(VideoFail, 1000);
	chrome.tabs.executeScript(myActiveTabId, {file: "worker_proxy.js"});
	chrome.tabs.executeScript(myActiveTabId, {file: "content_script.js"});	
}

function setMode(aTabId, aMode)
{
	myCurrentMode = aMode;
	if ((myActiveTabId == aTabId && myCurrentMode == "disabled" ) || (myActiveTabId != aTabId && myCurrentMode == "enabled"))
	{
		if (myActiveTabId!=-1)
		{
			tabMessage(myActiveTabId, {cmd: "setmode", mode :"disabled"});
			myActiveTabId = -1;
		}
	}
	
	if (myCurrentMode == "enabled")
	{
		myActiveTabId = aTabId;
		if (myTabHistory.indexOf(myActiveTabId)==-1)//never seen the tab before, we know we need to inject
		{
			myTabHistory.push(myActiveTabId);
			inject();
		}
		else //we've seen the tab before, test it
		{
			modeTimeout = setTimeout( inject, 1000);
			tabMessage(myActiveTabId, {cmd: "setmode", mode :"enabled"});
		}

	}
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

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
	if(request.cmd === "onload")
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
	else if (request.cmd === "modeset")
	{
		clearTimeout(modeTimeout);
	}
 });

chrome.browserAction.onClicked.addListener(function (tab){toggleEnabled(tab);});	
getSettings();

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
	});
}

