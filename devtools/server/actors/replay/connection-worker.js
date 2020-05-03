/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// Two sockets can be opened with the Web Replay cloud service.
// gServerSocket is used for normal messages, and gUploadSocket is used for
// uploading blocks of binary data. Two sockets are used so that the
// normal socket isn't blocked when uploading large amounts of data.
let gServerSocket;
let gUploadSocket;

// The upload socket is created lazily.
let gUploadOpen;
const gPendingUploadMessages = [];

let gConfig;

self.addEventListener("message", makeInfallible(onMainThreadMessage));

function onMainThreadMessage({ data }) {
  switch (data.kind) {
    case "initialize":
      gConfig = data;
      openServerSocket();
      break;
    case "sendCommand":
      doSend(gServerSocket, JSON.stringify(data.command));
      break;
    case "sendUploadCommand":
      ensureUploadSocket();
      if (gUploadOpen) {
        doSend(gUploadSocket, JSON.stringify(data.command));
      } else {
        gPendingUploadMessages.push(JSON.stringify(data.command));
      }
      break;
    case "sendUploadBinaryData":
      ensureUploadSocket();
      if (gUploadOpen) {
        doSend(gUploadSocket, data.buf);
      } else {
        gPendingUploadMessages.push(data.buf);
      }
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

function ensureUploadSocket() {
  if (gUploadSocket) {
    return;
  }
  gUploadSocket = new WebSocket(gConfig.address);
  gServerSocket.onopen = makeInfallible(onUploadOpen);
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

function onUploadOpen(evt) {
  gUploadOpen = true;
  gPendingUploadMessages.forEach(msg => doSend(gUploadSocket, msg));
  gPendingUploadMessages.length = 0;
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
  } else {
    postMessage({ kind: "commandResult", id: data.id, result: data.result });
  }
}

const doSend = makeInfallible((socket, msg) => socket.send(msg));

function makeInfallible(fn, thisv) {
  return (...args) => {
    try {
      fn.apply(thisv, args);
    } catch (e) {
      postError(e);
    }
  };
}
