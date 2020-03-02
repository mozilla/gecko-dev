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
    default:
      PostError(`Unknown event kind ${data.kind}`);
  }
});

function openServerSocket() {
  gServerSocket = new WebSocket(gServerAddress);
  gServerSocket.onopen = onServerOpen;
  gServerSocket.onclose = onServerClose;
  gServerSocket.onmessage = onServerMessage;
  gServerSocket.onerror = onServerError;
}

// Status of the cloud server connection.
let gStatus = "cloudInitialize.label";

function updateStatus(status) {
  gStatus = status;
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
  updateStatus("cloudReconnecting.label");
  setTimeout(openServerSocket, 3000);
  doLog(`CloudServer Connection Closed\n`);
}

function onServerError(evt) {
  updateStatus("cloudError.label");
  doLog(`CloudServer Connection Error\n`);
}

async function onServerMessage(evt) {
  try {
    const data = JSON.parse(evt.data);
    switch (data.kind) {
      case "modules": {
        const { sessionId, controlJS, replayJS, updateNeeded, updateWanted } = data;
        if (updateNeeded) {
          updateStatus("cloudUpdateNeeded.label");
        } else {
          postMessage({ kind: "loaded", sessionId, controlJS, replayJS });
          updateStatus("");
        }
        if (updateNeeded || updateWanted) {
          postMessage({ kind: "downloadUpdate", updateNeeded });
        }
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

async function doConnect(id, channelId) {
  if (gConnections[id]) {
    PostError(`Duplicate connection ID ${id}`);
  }
  const connection = {
    // Messages to send to the replayer.
    outgoing: [],

    // Whether the replayer connection is fully established. When set,
    // parent process logs will go to the replayer instead of the main server.
    connected: false,

    // Resolve hook for any promise waiting on the socke to connect.
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
        PostError(`Send error ${e}`);
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
  postMessage({ kind: "disconnected", id });
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
  // When we have heard back from the replayer, we are fully connected to it.
  if (!gConnections[id].connected) {
    gConnections[id].connected = true;
    postMessage({ kind: "connected", id });
  }

  gMessages.push({ id, promise: evt.data.arrayBuffer() });
  if (gMessageWaiter) {
    gMessageWaiter();
    gMessageWaiter = null;
  }
}

function onError(id, evt) {
  PostError("ReplaySocketError", id);
}

function PostError(why) {
  postMessage({ kind: "error", why: why.toString() });
}

function doLog(text) {
  if (gStatus) {
    sendMessageToCloudServer({ kind: "log", text });
  } else {
    postMessage({ kind: "logOffline", text });
  }
}
