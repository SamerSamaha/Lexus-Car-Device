// A Hub button on every page of the car browser (LHU-055, DN-055): a 10 mm round target in the
// top-left corner, drawn in a shadow root so that the page's styles cannot change it, and moved
// into the element that is full screen (a video), so that it stays visible there too. One tap
// asks background.js to close the window. 10 mm is 116 px on the 5-inch panel (D-024) at the
// browser's device scale of 1.
"use strict";

(function () {
  if (window.top !== window || document.getElementById("lexus-hub-return")) {
    return;
  }
  var sizePixels = 116;
  var host = document.createElement("div");
  host.id = "lexus-hub-return";
  host.style.cssText = "position:fixed;left:12px;top:12px;width:" + sizePixels + "px;height:" +
    sizePixels + "px;z-index:2147483647;";
  var shadow = host.attachShadow({ mode: "closed" });
  var button = document.createElement("button");
  button.type = "button";
  button.textContent = "Hub";
  button.setAttribute("aria-label", "Back to the hub");
  button.style.cssText = "all:initial;box-sizing:border-box;width:100%;height:100%;" +
    "border-radius:50%;border:3px solid #f4f6f8;background:rgba(16,20,24,0.78);" +
    "color:#f4f6f8;font:700 30px system-ui,sans-serif;text-align:center;cursor:pointer;";
  shadow.appendChild(button);

  button.addEventListener("click", function (event) {
    event.preventDefault();
    event.stopPropagation();
    button.textContent = "...";
    chrome.runtime.sendMessage({ lexusHubReturn: true });
  });

  function place() {
    var parent = document.fullscreenElement || document.documentElement;
    if (host.parentNode !== parent) {
      parent.appendChild(host);
    }
  }

  document.addEventListener("fullscreenchange", place);
  place();
})();
