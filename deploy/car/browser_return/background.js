// The Hub button of content.js asks for this window to close (LHU-055, DN-055). The car
// browser has one window and a profile of its own, so closing it ends the browser process; the
// hub sees its app exit and is in front again (REQ-016).
"use strict";

chrome.runtime.onMessage.addListener(function (message, sender) {
  if (message && message.lexusHubReturn === true && sender.tab) {
    chrome.windows.remove(sender.tab.windowId);
  }
});
