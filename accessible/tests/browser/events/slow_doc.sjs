/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// stolen from file_blocked_script.sjs
function setGlobalState(key, data) {
  const x = {
    data,
    QueryInterface: ChromeUtils.generateQI([]),
  };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key) {
  let data;
  getObjectState(key, function (x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

function handleRequest(request, response) {
  const second = request.queryString == "second";
  const secondInContainer = request.queryString == "secondInContainer";
  if (second || secondInContainer) {
    // Initial request for document.
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/html", false);
    response.write("<!doctype html>");
    response.write('<p id="first">first</p>');
    if (secondInContainer) {
      response.write("<section>");
    }
    // Reference a slow script which will block parsing the rest of this
    // document.
    response.write('<script src="?script"></script>');
    response.write('<p id="second">second</p>');
    if (secondInContainer) {
      response.write("</section>");
    }
    return;
  }

  if (request.queryString == "script") {
    // This is the request to load the slow, blocking script.
    response.processAsync();
    // Store the response in the global state.
    setGlobalState("a11y-script-response", response);
    const resolve = getGlobalState("a11y-script-resolve");
    if (resolve) {
      // Sometimes, the request to finish loading the script can arrive before
      // the initial request to load the script, since they are requested from
      // different processes. Notify the finish request that the initial request
      // has been handled.
      resolve();
    }
    return;
  }

  if (request.queryString == "scriptFinish") {
    // This is the request to finish (unblock) the slow script.
    response.processAsync();
    const finish = () => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Cache-Control", "no-cache", false);
      response.setHeader("Content-Type", "application/json", false);
      response.write("true"); // the payload doesn't matter.
      response.finish();
      const blockedResponse = getGlobalState("a11y-script-response");
      blockedResponse.setStatusLine(request.httpVersion, 200, "OK");
      blockedResponse.setHeader("Cache-Control", "no-cache", false);
      blockedResponse.setHeader("Content-Type", "text/javascript", false);
      blockedResponse.write(";"); // the payload doesn't matter.
      blockedResponse.finish();
      setGlobalState("a11y-script-response", undefined);
      setGlobalState("a11y-script-resolve", undefined);
    };
    if (getGlobalState("a11y-script-response")) {
      // The initial script request has already been handled.
      finish();
    } else {
      // This finish request arrived before the initial request. Set up a
      // Promise so we know when the initial request has been handled.
      const promise = Promise.withResolvers();
      promise.promise.then(finish);
      setGlobalState("a11y-script-resolve", promise.resolve);
    }
  }
}
