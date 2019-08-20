'use strict';

const script = document.createElement('script');
script.setAttribute("type", "module");
script.setAttribute("src", chrome.extension.getURL('content_script.js'));

const head = document.head || document.getElementsByTagName("head")[0] || document.documentElement;
head.insertBefore(script, head.lastChild);

script.onload = function(){
  var url=chrome.extension.getURL('');
  var evt=document.createEvent("CustomEvent");
  evt.initCustomEvent("extensionUrl", true, true, url);
  document.dispatchEvent(evt);
};

