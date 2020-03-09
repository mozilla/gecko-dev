/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

let gServerSocket;
const gConnections = [];

let gServerAddress;
let gBuildId;

self.addEventListener("message", msg => {
  try {
    onMainThreadMessage(msg);
  } catch (e) {
    PostError(e);
  }
});

function onMainThreadMessage({ data }) {
  switch (data.kind) {
    case "initialize":
      gServerAddress = data.address;
      gBuildId = data.buildId;
      openServerSocket();
    case "connect":
      doConnect(data.id, data.channelId);
      break;
    case "send":
      doSend(data.id, data.buf);
      break;
    case "log":
      doLog(data.text);
      break;
    default:
      PostError(`Unknown event kind ${data.kind}`);
  }
}

function openServerSocket() {
  gServerSocket = new WebSocket(gServerAddress);
  gServerSocket.onopen = onServerOpen;
  gServerSocket.onclose = onServerClose;
  gServerSocket.onmessage = onServerMessage;
  gServerSocket.onerror = onServerError;
}

// Whether log messages can be sent to the cloud server.
let gConnected = false;

function updateStatus(status) {
  postMessage({ kind: "updateStatus", status });
}

function sendMessageToCloudServer(msg) {
  gServerSocket.send(JSON.stringify(msg));
}

function onServerOpen(evt) {
  sendMessageToCloudServer({ kind: "initialize", buildId: gBuildId });
  updateStatus("cloudInitialize.label");
}

function onServerClose() {
  gConnected = false;
  updateStatus("cloudReconnecting.label");
  setTimeout(openServerSocket, 3000);
  doLog(`CloudServer Connection Closed\n`);
}

function onServerError(evt) {
  gConnected = false;
  updateStatus("cloudError.label");
  doLog(`CloudServer Connection Error\n`);
}

async function onServerMessage(evt) {
  try {
    const data = JSON.parse(evt.data);
    switch (data.kind) {
      case "modules": {
        const { sessionId, controlJS, replayJS, updateNeeded, updateWanted } = data;
        gConnected = true;
        postMessage({ kind: "loaded", sessionId, controlJS, replayJS, updateNeeded, updateWanted });
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
    PostError(e);
  }
}

// How much time to give a new connection to establish itself before closing it
// and reattempting to connect.
const SocketTimeoutMs = 5000;

async function doConnect(id, channelId) {
  if (gConnections[id]) {
    PostError(`Duplicate connection ID ${id}`);
  }
  const connection = {
    // Messages to send to the replayer.
    outgoing: [],

    // Whether the main socket is fully established.
    connected: false,

    // Whether the bulk socket is open.
    bulkOpen: false,

    // Resolve hook for any promise waiting on the socket to connect.
    connectWaiter: null,

    // Resolve hook for any promise waiting on new outgoing messages.
    sendWaiter: null,
  };
  gConnections[id] = connection;

  sendMessageToCloudServer({ kind: "connect", id });

  const address = await new Promise(resolve => (connection.connectWaiter = resolve));

  if (!/^wss?:\/\//.test(address)) {
    PostError(`Invalid websocket address ${text}`);
  }

  // Eventually this ID will include credentials.
  const sessionId = (Math.random() * 1e9) | 0;

  const socket = new WebSocket(`${address}/connect?id=${sessionId}`);
  socket.onopen = evt => onOpen(id);
  socket.onclose = evt => onClose(id);
  socket.onmessage = evt => onMessage(id, evt);
  socket.onerror = evt => onError(id);

  const bulkSocket = new WebSocket(`${address}/connect?id=${sessionId}&bulk=true`);
  bulkSocket.onopen = evt => onOpen(id, true);
  bulkSocket.onclose = evt => onClose(id);
  bulkSocket.onmessage = evt => onMessage(id, evt, true);
  bulkSocket.onerror = evt => onError(id);

  setTimeout(() => {
    if (!connection.connected) {
      socket.close();
      bulkSocket.close();
    }
  }, SocketTimeoutMs);

  await new Promise(resolve => (connection.openWaiter = resolve));

  while (gConnections[id]) {
    if (connection.outgoing.length) {
      const buf = connection.outgoing.shift();
      try {
        const bulk = checkCompleteMessage(buf);
        if (bulk && connection.bulkOpen) {
          bulkSocket.send(buf);
        } else {
          socket.send(buf);
        }
      } catch (e) {
        PostError(`Send error ${e}`);
      }
    } else {
      await new Promise(resolve => (connection.sendWaiter = resolve));
    }
  }
}

function readMessage(msg, offset = 0) {
  if (offset + 4 > msg.length) {
    return null;
  }
  const bulk = msg[offset];
  const size = msg[offset + 1] | (msg[offset + 2] << 8) | (msg[offset + 3] << 16);
  return { bulk, size };
}

function checkCompleteMessage(buf) {
  if (buf.byteLength < 4) {
    PostError(`Message too short`);
  }
  const { bulk, size } = readMessage(new Uint8Array(buf));
  if (size != buf.byteLength) {
    PostError(`Message not complete`);
  }
  return bulk;
}

function doSend(id, buf) {
  const connection = gConnections[id];
  connection.outgoing.push(buf);
  if (connection.sendWaiter) {
    connection.sendWaiter();
    connection.sendWaiter = null;
  }
}

function onOpen(id, bulk) {
  // Messages can now be sent to the socket.
  if (bulk) {
    gConnections[id].bulkOpen = true;
  } else {
    gConnections[id].openWaiter();
  }
}

function onClose(id, bulk) {
  postMessage({ kind: "disconnected", id });
  gConnections[id] = null;
}

function arrayToString(arr) {
  let str = "";
  for (const n of arr) {
    str += String.fromCharCode(n);
  }
  return escape(str);
}

// Buffers containing incomplete messages.
const partialMessages = new Map();

function extractCompleteMessages(id, bulk, data) {
  const key = `${id}:${bulk}`;
  const oldData = partialMessages.get(key);
  if (oldData) {
    partialMessages.delete(key);
    const newData = new Uint8Array(oldData.length + data.length);
    newData.set(oldData);
    newData.set(data, oldData.length);
    data = newData;
  }

  const messages = [];
  let offset = 0;
  while (true) {
    const info = readMessage(data, offset);
    if (!info || offset + info.size > data.length) {
      if (offset < data.length) {
        partialMessages.set(key, new Uint8Array(data.buffer, offset));
      }
      break;
    }
    // Copy the message into its own ArrayBuffer.
    const msg = new Uint8Array(info.size);
    msg.set(new Uint8Array(data.buffer, offset, info.size));
    messages.push(msg.buffer);
    offset += info.size;
  }
  return messages;
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
      const { id, bulk, promise } = gMessages.shift();
      const buf = await promise;
      const messages = extractCompleteMessages(id, bulk, new Uint8Array(buf));
      for (const msg of messages) {
        postMessage({ kind: "message", id, buf: msg });
      }
    } else {
      await new Promise(resolve => (gMessageWaiter = resolve));
    }
  }
})();

function onMessage(id, evt, bulk) {
  // When we have heard back from the replayer, we are fully connected to it.
  if (!gConnections[id].connected && !bulk) {
    gConnections[id].connected = true;
    postMessage({ kind: "connected", id });
  }

  gMessages.push({ id, bulk, promise: evt.data.arrayBuffer() });
  if (gMessageWaiter) {
    gMessageWaiter();
    gMessageWaiter = null;
  }
}

function onError(id, evt) {
  PostError("ReplaySocketError", id);
}

function PostError(why, id) {
  postMessage({ kind: "error", why: why.toString(), id });
}

function doLog(text) {
  if (gConnected) {
    sendMessageToCloudServer({ kind: "log", text });
  } else {
    postMessage({ kind: "logOffline", text });
  }
}
