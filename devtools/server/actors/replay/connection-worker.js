/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// Main socket for communicating with the Web Replay cloud service.
let gServerSocket;

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
      getUploadSocket(data.pid).send(JSON.stringify(data.command));
      break;
    case "sendUploadBinaryData":
      getUploadSocket(data.pid).send(data.buf);
      break;
    case "stopUpload":
      destroyUploadSocket(data.pid);
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

// Every upload uses its own socket. This allows other communication with the
// cloud service even if the upload socket has a lot of pending data to send.
function UploadSocket() {
  this.socket = new WebSocket(gConfig.address);
  this.socket.onopen = makeInfallible(() => this.onOpen());
  this.socket.onclose = makeInfallible(() => this.onClose());
  this.socket.onmessage = makeInfallible(onServerMessage);
  this.socket.onerror = makeInfallible(() => this.onError());

  this.open = false;
  this.pending = [];

  this.closed = false;
}

UploadSocket.prototype = {
  onOpen() {
    this.open = true;
    this.pending.forEach(msg => doSend(this.socket, msg));
    this.pending.length = 0;
  },

  onClose() {
    if (!this.closed) {
      onServerClose();
    }
  },

  onError() {
    if (!this.closed) {
      onServerError();
    }
  },

  send(msg) {
    if (this.open) {
      doSend(this.socket, msg);
    } else {
      this.pending.push(msg);
    }
  },

  close() {
    this.closed = true;
    this.socket.close();
  },
};

const gUploadSockets = new Map();

function getUploadSocket(pid) {
  if (!gUploadSockets.has(pid)) {
    gUploadSockets.set(pid, new UploadSocket());
  }
  return gUploadSockets.get(pid);
}

function destroyUploadSocket(pid) {
  if (gUploadSockets.has(pid)) {
    gUploadSockets.get(pid).close();
    gUploadSockets.delete(pid);
  }
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

function onServerError() {
  updateStatus("cloudError.label");
}

function onServerMessage(evt) {
  const data = JSON.parse(evt.data);
  if (data.error) {
    dump(`ServerError ${JSON.stringify(data)}\n`);
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
