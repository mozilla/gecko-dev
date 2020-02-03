/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// This file provides an interface for connecting middleman processes with
// replaying processes living remotely in the cloud.

let gCloudAddress;
let gWorker;
let gStatusCallback;
let gLoadedCallback;
let gMessageCallback;
let gNextConnectionId = 1;

// eslint-disable-next-line no-unused-vars
function Initialize(address, statusCallback, loadedCallback, messageCallback) {
  gWorker = new Worker("connection-worker.js");
  gWorker.addEventListener("message", onMessage);
  gStatusCallback = statusCallback;
  gLoadedCallback = loadedCallback;
  gMessageCallback = messageCallback;

  gWorker.postMessage({ type: "initialize", address });
}

function onMessage(evt) {
  switch (evt.data.kind) {
    case "updateStatus":
      gStatusCallback(evt.data.status);
      break;
    case "loaded":
      gLoadedCallback(evt.data.controlJS, evt.data.replayJS);
      break;
    case "message":
      gMessageCallback(evt.data.id, evt.data.buf);
      break;
  }
}

// eslint-disable-next-line no-unused-vars
function Connect(channelId, callback) {
  const id = gNextConnectionId++;
  gWorker.postMessage({ type: "connect", id, channelId });
  return id;
}

// eslint-disable-next-line no-unused-vars
function SendMessage(id, buf) {
  gWorker.postMessage({ type: "send", id, buf });
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["Initialize", "Connect", "SendMessage"];
