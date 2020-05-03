/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

let gServerSocket;
const gConnections = [];

let gConfig;

self.addEventListener("message", makeInfallible(onMainThreadMessage));

function onMainThreadMessage({ data }) {
  switch (data.kind) {
    case "initialize":
      gConfig = data;
      openServerSocket();
      break;
    default:
      postError(`Unknown event kind ${data.kind}`);
  }
}

function openServerSocket() {
  gServerSocket = new WebSocket(gConfig.address);
  gServerSocket.onopen = makeInfallible(onServerOpen);
  gServerSocket.onclose = makeInfallible(onServerClose);
  gServerSocket.onmessage = makeInfallible(onServerMessage);
  gServerSocket.onerror = makeInfallible(onServerError);
}

function updateStatus(status) {
  postMessage({ kind: "updateStatus", status });
}

function sendMessageToCloudServer(msg) {
  gServerSocket.send(JSON.stringify(msg));
}

function onServerOpen(evt) {
  updateStatus("");
}

function onServerClose() {
  updateStatus("cloudReconnecting.label");
  setTimeout(openServerSocket, 3000);
}

function onServerError(evt) {
  updateStatus("cloudError.label");
}

function onServerMessage(evt) {
  const data = JSON.parse(evt.data);
  if (data.error) {
    updateStatus("cloudError.label");
  }
}

function makeInfallible(fn, thisv) {
  return (...args) => {
    try {
      fn.apply(thisv, args);
    } catch (e) {
      postError(e);
    }
  };
}
