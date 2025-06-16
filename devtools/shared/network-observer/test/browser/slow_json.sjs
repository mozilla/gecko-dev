/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// JSON contains a multibyte character
const JSON_BYTES_1 = '"\xE3';
const JSON_BYTES_2 = '\x81\x82"';

// stolen from file_blocked_script.sjs
function setGlobalState(data, key) {
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
  if (/^complete/.test(request.queryString)) {
    // Unblock the previous request.
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "application/json", false);
    response.write("true"); // the payload doesn't matter.

    const blockedResponse = getGlobalState("devtools-webconsole");
    if (blockedResponse) {
      blockedResponse.write(JSON_BYTES_2);
      blockedResponse.finish();

      setGlobalState(undefined, "devtools-webconsole");
    }
  } else {
    // Getting the JSON
    const partial = /^partial/.test(request.queryString);
    if (!partial) {
      response.processAsync();
    }
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "application/json; charset=utf8", false);
    response.write(JSON_BYTES_1);
    if (!partial) {
      // Store the response in the global state
      setGlobalState(response, "devtools-webconsole");
      response.bodyOutputStream.flush();
    }
  }
}
