/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop*/

var loop = loop || {};
loop.panel = (function() {
  "use strict";

  // XXX: baseApiUrl should be configurable (browser pref)
  var baseApiUrl = "http://localhost:5000";

  function show(el) {
    el.classList.remove("hide");
  }

  function hide(el) {
    el.classList.add("hide");
  }

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
    document.querySelector(".share .messages").appendChild(errEl);
  }

  function clearNotifications() {
    document.querySelector(".share .messages .alert").remove();
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
    var shareEl = document.querySelector(".share"),
        actionEl = shareEl.querySelector(".action")
    clearNotifications();
    hide(actionEl.querySelector(".invite"));
    actionEl.querySelector(".result input").value = callUrl;
    show(actionEl.querySelector(".result"));
    shareEl.querySelector(".description p").textContent =
      document.mozL10n.get("share_link_url");
  }

  function init() {
    document.querySelector(".get-url").addEventListener("click",
      function(event) {
        event.preventDefault();
        requestCallUrl(function(err, callUrlData) {
          if (err) {
            console.error("Unable to retrieve call url", err);
            notifyError(document.mozL10n.get("unable_retrieve_url"));
            return;
          }
          if (typeof callUrlData !== "object" ||
              !callUrlData.hasOwnProperty("call_url")) {
            console.error("Invalid call url data received", callUrlData);
            notifyError(document.mozL10n.get("unable_retrieve_url"));
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
})();
