var settings;

function restore_options()
{
	chrome.runtime.sendMessage({cmd: "defaultSettings"});
	var status1 = document.getElementById('status1');
	status1.textContent = 'Options restored.';
	var status2 = document.getElementById('status2');
	status2.textContent = 'Options restored.';
	setTimeout(function() {
		status1.textContent = '';
		status2.textContent = '';
	}, 750);
}

chrome.storage.onChanged.addListener(function(changes, namespace) {
	load_options();
});

function save_options() {
	var trimmed = document.getElementById('cardpool').value.replace(/^\d+\s*/gm, '');
	if (trimmed.length<3)
	{
		trimmed = "All Cards & Tokens";
	}
	document.getElementById('cardpool').value = trimmed;
	
  chrome.storage.sync.set({
    debugview: document.getElementById('debugview').checked,
	mousemode : document.getElementById('mousemode').value,
	mouseanchor : document.getElementById('mouseanchor').checked,
	vertical : document.getElementById('vertical').value,
	offsety : Number(document.getElementById('offsety').value),
	horizontal : document.getElementById('horizontal').value,
	offsetx : Number(document.getElementById('offsetx').value),
	clickareaheight : Number(document.getElementById('clickareaheight').value),
	clickareawidth : Number(document.getElementById('clickareawidth').value),
	clickareax : Number(document.getElementById('clickareax').value),
	clickareay : Number(document.getElementById('clickareay').value),
	mincardsize : Number(document.getElementById('mincardsize').value),
	maxcardsize : Number(document.getElementById('maxcardsize').value),
	goodscore : Number(document.getElementById('goodscore').value),
	okscore : Number(document.getElementById('okscore').value),
	detailsTargetWidth : Number(document.getElementById('detailsTargetWidth').value),
	autoscreen:document.getElementById('autoscreen').checked,
	automatchtimeout : Number(document.getElementById('automatchtimeout').value),
	automatchhistorysize : Number(document.getElementById('automatchhistorysize').value),
	automatchwidth : Number(document.getElementById('automatchwidth').value),
	automatchheight : Number(document.getElementById('automatchheight').value),
	automatchx : Number(document.getElementById('automatchx').value),
	automatchy : Number(document.getElementById('automatchy').value),
	autocolor : document.getElementById('autocolor').checked,
	canvascolor : document.getElementById('canvascolor').checked,
	tooltiplogo : document.getElementById('tooltiplogo').checked,
	showhistory : document.getElementById('showhistory').checked,
	tooltipscryfall : document.getElementById('tooltipscryfall').checked,
	showsavebutton : document.getElementById('showsavebutton').checked,
	resetmouseoffset : document.getElementById('resetmouseoffset').checked,
		cardpool : trimmed
  }, function() {
    var status1 = document.getElementById('status1');
    status1.textContent = 'Options saved.';
    var status2 = document.getElementById('status2');
    status2.textContent = 'Options saved.';
    setTimeout(function() {
      status1.textContent = '';
      status2.textContent = '';
    }, 750);
  });
}

function setDocumentValues(items)
{
		document.getElementById('debugview').checked 			= items.debugview;
    document.getElementById('mousemode').value 				= items.mousemode;
    document.getElementById('mouseanchor').checked 			= items.mouseanchor;
    document.getElementById('vertical').value 				= items.vertical;
    document.getElementById('offsety').value 				= items.offsety;
    document.getElementById('horizontal').value 			= items.horizontal;
    document.getElementById('offsetx').value 				= items.offsetx;
    document.getElementById('clickareaheight').value 			= items.clickareaheight;
    document.getElementById('clickareawidth').value 			= items.clickareawidth;
    document.getElementById('clickareax').value 			= items.clickareax;
    document.getElementById('clickareay').value 			= items.clickareay;
    document.getElementById('mincardsize').value 			= items.mincardsize;
    document.getElementById('maxcardsize').value 			= items.maxcardsize;
    document.getElementById('goodscore').value 				= items.goodscore;
    document.getElementById('okscore').value 				= items.okscore;
    document.getElementById('detailsTargetWidth').value 	= items.detailsTargetWidth;
    document.getElementById('autoscreen').checked 			= items.autoscreen;
    document.getElementById('automatchtimeout').value 		= items.automatchtimeout;
    document.getElementById('automatchhistorysize').value 	= items.automatchhistorysize;
    document.getElementById('automatchwidth').value 		= items.automatchwidth;
    document.getElementById('automatchheight').value 		= items.automatchheight;
    document.getElementById('automatchx').value 			= items.automatchx;
    document.getElementById('automatchy').value 			= items.automatchy;
    document.getElementById('autocolor').checked 			= items.autocolor;
    document.getElementById('canvascolor').checked 			= items.canvascolor;
    document.getElementById('showhistory').checked 			= items.showhistory;
    document.getElementById('tooltiplogo').checked 			= items.tooltiplogo;
    document.getElementById('tooltipscryfall').checked 		= items.tooltipscryfall;
		document.getElementById('showsavebutton').checked 		= items.showsavebutton;
		document.getElementById('resetmouseoffset').checked 		= items.resetmouseoffset;
		var trimmed = items.cardpool;
		if (trimmed.length<3)
		{
			trimmed = "All Cards & Tokens";
		}
		document.getElementById('cardpool').value = trimmed;
}

function load_options()
{
	setTimeout(function() {
		chrome.runtime.sendMessage({cmd: "getSettings"}, function(items) {
			settings = items;
			setDocumentValues(items);
			});
	}, 1000); //need to wait for background to have the settings... not pretty
	
}
document.addEventListener('DOMContentLoaded', load_options);
document.getElementById('save1').addEventListener('click', save_options);
document.getElementById('save2').addEventListener('click', save_options);
document.getElementById('restore1').addEventListener('click', restore_options);
	