/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop*/

var loop = loop || {};
loop.panel = (function(dom) {
  "use strict";

  // XXX: baseApiUrl should be configurable (browser pref)
  var baseApiUrl = "http://localhost:5000";

  function  httpError(req) {
    var reason = req.responseText || "Unknown reason.";
    return new Error("Failed HTTP request: " + req.status + "; " + reason);
  }

  function notifyError(msg) {
    var errEl = document
                  .querySelector("#error-notification")
                  .content
                  .cloneNode(true);
    errEl.querySelector("p").textContent = msg;
    dom.appendEl(errEl, ".share .messages");
  }

  function clearNotifications() {
    dom.removeEl(".share .messages .alert");
  }

  // XXX: abstract XHR for further reusability or YAGNI?
  function requestCallUrl(cb) {
    var request = new XMLHttpRequest();

    request.onload = function(event) {
      if (request.readyState === 4  && request.status === 200) {
        try {
          cb(null, JSON.parse(request.responseText));
        } catch (err) {
          cb(new Error("Invalid JSON received; " + err));
        }
        return;
      } else {
        cb(httpError(request));
      }
    };

    request.onerror = request.ontimeout = function(event) {
      cb(httpError(event.target));
    };

    request.open("POST", baseApiUrl + "/call-url/", true);
    request.setRequestHeader("Content-Type", "application/json");
    request.send(JSON.stringify({
      simplepushUrl: "temporarily_invalid" // XXX: waiting for simplepush impl.
    }));
  }

  function onCallUrlReceived(callUrl) {
    clearNotifications();
    dom.hideEl(".share .action .invite");
    dom.setElValue(".share .action .result input", callUrl);
    dom.showEl(".share .action .result");
    dom.setElText(".share .description p",
                  "Share the link below with your friend to start your call!");
  }

  function init() {
    document.querySelector(".get-url").addEventListener("click",
      function(event) {
        event.preventDefault();
        requestCallUrl(function(err, callUrlData) {
          if (err) {
            console.error("Unable to retrieve call url", err);
            notifyError("Sorry, we were unable to retrieve a call url.");
            return;
          }
          if (typeof callUrlData !== "object" ||
              !callUrlData.hasOwnProperty("call_url")) {
            console.error("Invalid call url data received", callUrlData);
            notifyError("Sorry, we were unable to retrieve a call url.");
            return;
          }
          onCallUrlReceived(callUrlData.call_url);
        });
      });
  }

  return {
    init: init,
    requestCallUrl: requestCallUrl
  };
})(loop.dom || {});
