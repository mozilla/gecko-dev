/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// Sockets for communicating with the Record Replay cloud service.
const gUploadSockets = new Map();

self.addEventListener("message", makeInfallible(onMainThreadMessage));

function onMainThreadMessage({ data }) {
  switch (data.kind) {
    case "openChannel":
      openUploadSocket(data.id, data.address);
      break;
    case "sendCommand":
      gUploadSockets.get(data.id).send(JSON.stringify(data.command));
      break;
    case "sendBinaryData":
      gUploadSockets.get(data.id).send(data.buf);
      break;
    case "closeChannel":
      destroyUploadSocket(data.id);
      break;
    default:
      postError(`Unknown event kind ${data.kind}`);
  }
}

// Every upload uses its own socket. This allows other communication with the
// cloud service even if the upload socket has a lot of pending data to send.
function UploadSocket(address) {
  this.address = address;
  this.open = false;
  this.pending = [];
  this.closed = false;

  this.initialize();
}

UploadSocket.prototype = {
  initialize() {
    this.socket = new WebSocket(this.address);
    this.socket.onopen = makeInfallible(() => this.onOpen());
    this.socket.onclose = makeInfallible(() => this.onClose());
    this.socket.onmessage = makeInfallible(onServerMessage);
    this.socket.onerror = makeInfallible(() => this.onError());
  },

  onOpen() {
    this.open = true;
    this.pending.forEach((msg) => doSend(this.socket, msg));
    this.pending.length = 0;

    updateStatus("");
  },

  onClose() {
    if (!this.closed) {
      updateStatus("cloudReconnecting.label");
      setTimeout(() => this.initialize(), 3000);
    }
  },

  onError() {
    if (!this.closed) {
      updateStatus("cloudError.label");
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

function openUploadSocket(id, address) {
  gUploadSockets.set(id, new UploadSocket(address));
}

function destroyUploadSocket(id) {
  if (gUploadSockets.has(id)) {
    gUploadSockets.get(id).close();
    gUploadSockets.delete(id);
  }
}

function updateStatus(status) {
  postMessage({ kind: "updateStatus", status });
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

function postError(msg) {
  dump(`Error: ${msg}\n`);
}
