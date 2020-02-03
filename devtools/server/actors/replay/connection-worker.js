/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

let gServerSocket;
const gConnections = [];

let gServerAddress;

self.addEventListener("message", function({ data }) {
  switch (data.type) {
    case "initialize":
      try {
        gServerAddress = data.address;
        openServerSocket();
      } catch (e) {
        ThrowError(e);
      }
    case "connect":
      try {
        doConnect(data.id, data.channelId);
      } catch (e) {
        ThrowError(e);
      }
      break;
    case "send":
      try {
        doSend(data.id, data.buf);
      } catch (e) {
        ThrowError(e);
      }
      break;
    default:
      ThrowError(`Unknown event type ${type}`);
  }
});

function openServerSocket() {
  gServerSocket = new WebSocket(gServerAddress);
  gServerSocket.onopen = onServerOpen;
  gServerSocket.onclose = onServerClose;
  gServerSocket.onmessage = onServerMessage;
  gServerSocket.onerror = onServerError;
}

async function openServerSocket(address) {
  const response = await fetch(address);
  const text = await response.text();

  if (!/^wss?:\/\//.test(text)) {
    ThrowError(`Invalid websocket address ${text}`);
  }

  gServerSocket = new WebSocket(text);
  gServerSocket.onopen = onServerOpen;
  gServerSocket.onclose = onServerClose;
  gServerSocket.onmessage = onServerMessage;
  gServerSocket.onerror = onServerError;
}

function updateStatus(status) {
  postMessage({ kind: "updateStatus", status });
}

function onServerOpen(evt) {
  const msg = { kind: "initialize" };
  gServerSocket.send(JSON.stringify(msg));
  updateStatus("cloudInitialize.label");
}

function onServerClose() {
  dump(`CloudServer Connection Closed\n`);

  updateStatus("cloudReconnecting.label");
  setTimeout(openServerSocket, 5000);
}

function onServerError(evt) {
  dump(`CloudServer Connection Error\n`);
  updateStatus("cloudError.label");
}

async function onServerMessage(evt) {
  try {
    const data = JSON.parse(evt.data);
    switch (data.kind) {
      case "modules": {
        const { controlJS, replayJS } = data;
        postMessage({ kind: "loaded", controlJS, replayJS });
        updateStatus("");
        break;
      }
      case "connectionAddress": {
        const { address, id } = data;
        gConnections[id].connectWaiter(address);
        break;
      }
      case "connectionFailed":
        postMessage({ kind: "connectionFailed" });
        break;
    }
  } catch (e) {
    ThrowError(e);
  }
}

async function doConnect(id, channelId) {
  if (gConnections[id]) {
    ThrowError(`Duplicate connection ID ${id}`);
  }
  const connection = { outgoing: [] };
  gConnections[id] = connection;

  const msg = { kind: "connect", id };
  gServerSocket.send(JSON.stringify(msg));

  const address = await new Promise(resolve => (connection.connectWaiter = resolve));

  if (!/^wss?:\/\//.test(address)) {
    ThrowError(`Invalid websocket address ${text}`);
  }

  const socket = new WebSocket(address);
  socket.onopen = evt => onOpen(id, evt);
  socket.onclose = evt => onClose(id, evt);
  socket.onmessage = evt => onMessage(id, evt);
  socket.onerror = evt => onError(id, evt);

  await new Promise(resolve => (connection.openWaiter = resolve));

  while (gConnections[id]) {
    if (connection.outgoing.length) {
      const buf = connection.outgoing.shift();
      try {
        socket.send(buf);
      } catch (e) {
        ThrowError(`Send error ${e}`);
      }
    } else {
      await new Promise(resolve => (connection.sendWaiter = resolve));
    }
  }
}

function doSend(id, buf) {
  const connection = gConnections[id];
  connection.outgoing.push(buf);
  if (connection.sendWaiter) {
    connection.sendWaiter();
    connection.sendWaiter = null;
  }
}

function onOpen(id) {
  // Messages can now be sent to the socket.
  gConnections[id].openWaiter();
}

function onClose(id, evt) {
  gConnections[id] = null;
}

// Message data must be sent to the main thread in the order it was received.
// This is a bit tricky with web socket messages as they return data blobs,
// and blob.arrayBuffer() returns a promise such that multiple promises could
// be resolved out of order. Make sure this doesn't happen by using a single
// async frame to resolve the promises and forward them in order.
const gMessages = [];
let gMessageWaiter = null;
(async function processMessages() {
  while (true) {
    if (gMessages.length) {
      const { id, promise } = gMessages.shift();
      const buf = await promise;
      postMessage({ kind: "message", id, buf });
    } else {
      await new Promise(resolve => (gMessageWaiter = resolve));
    }
  }
})();

function onMessage(id, evt) {
  gMessages.push({ id, promise: evt.data.arrayBuffer() });
  if (gMessageWaiter) {
    gMessageWaiter();
    gMessageWaiter = null;
  }
}

function onError(id, evt) {
  ThrowError(`Socket error ${evt}`);
}

function ThrowError(msg) {
  dump(`Connection Worker Error: ${msg}\n`);
  throw new Error(msg);
}
