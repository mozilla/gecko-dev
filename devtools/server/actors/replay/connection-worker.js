/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

let gServerSocket;
const gConnections = [];

let gServerAddress;
let gBuildId;
let gVerbose;

ChromeUtils.recordReplayRegisterConnectionWorker(doSend);

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
      gVerbose = data.verbose;
      openServerSocket();
    case "connect":
      doConnect(data.id, data.channelId);
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

let gSessionId;

async function onServerMessage(evt) {
  try {
    const data = JSON.parse(evt.data);
    switch (data.kind) {
      case "modules": {
        const { sessionId, controlJS, replayJS, updateNeeded, updateWanted } = data;
        gConnected = true;
        gSessionId = sessionId;
        postMessage({ kind: "loaded", sessionId, controlJS, replayJS, updateNeeded, updateWanted });
        break;
      }
      case "connectionAddress": {
        const { id, replayerSessionId, address } = data;
        gConnections[id].replayerSessionId = replayerSessionId;
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

const MessageLogCount = 3;

function maybeLogMessage(prefix, id, message, count, delay) {
  if (gVerbose || count <= MessageLogCount) {
    const desc = messageDescription(message);
    const time = elapsedTime();
    const delayText = delay ? ` Delay ${roundTime(delay)}` : "";
    doLog(`${prefix} Connection ${id} Elapsed ${time}${delayText} Message ${desc}\n`);
  }
  if (!gVerbose && count == MessageLogCount) {
    doLog(`Verbose not set, not logging future ${prefix} messages for connection ${id}\n`);
  }
}

// How much time to give a new connection to establish itself before closing it
// and reattempting to connect.
const SocketTimeoutMs = 20000;

async function doConnect(id, channelId) {
  doLog(`ReplayerConnect ${id} ${elapsedTime()}\n`);

  if (gConnections[id]) {
    PostError(`Duplicate connection ID ${id}`);
  }
  const connection = {
    // ID assigned to this session by the dispatcher.
    replayerSessionId: null,

    // Websockets used by this connection.
    socket: null,
    bulkSocket: null,

    // Messages queued up before the main socket opened.
    outgoing: [],

    // Whether the main socket is open.
    open: false,

    // Whether the bulk socket is open.
    bulkOpen: false,

    // Whether the main socket is fully established.
    connected: false,

    // Resolve hook for any promise waiting on the socket to connect.
    connectWaiter: null,

    // How many messages have been sent/received over this connection.
    numSends: 0,
    numRecvs: 0,
  };
  gConnections[id] = connection;

  sendMessageToCloudServer({ kind: "connect", id });

  const address = await new Promise(resolve => (connection.connectWaiter = resolve));

  if (!/^wss?:\/\//.test(address)) {
    PostError(`Invalid websocket address ${text}`);
  }

  function urlParams(bulk) {
    const id = connection.replayerSessionId;
    return `id=${id}&dispatchId=${gSessionId}&bulk=${bulk}&verbose=${gVerbose}`;
  }

  const socket = new WebSocket(`${address}/connect?${urlParams(false)}`);
  socket.binaryType = "arraybuffer";
  socket.onopen = evt => onOpen(id);
  socket.onclose = evt => onClose(id);
  socket.onmessage = evt => onMessage(id, evt);
  socket.onerror = evt => onError(id);
  connection.socket = socket;

  const bulkSocket = new WebSocket(`${address}/connect?${urlParams(true)}`);
  bulkSocket.binaryType = "arraybuffer";
  bulkSocket.onopen = evt => onOpen(id, true);
  bulkSocket.onclose = evt => onClose(id);
  bulkSocket.onmessage = evt => onMessage(id, evt, true);
  bulkSocket.onerror = evt => onError(id);
  connection.bulkSocket = bulkSocket;

  setTimeout(() => {
    if (!connection.connected) {
      doLog(`ReplayerConnectionTimedOut ${id} ${elapsedTime()}\n`);
      socket.close();
      bulkSocket.close();
    }
  }, SocketTimeoutMs);
}

function readMessage(msg, offset = 0) {
  if (offset + 6 > msg.length) {
    return null;
  }
  const bulk = !!msg[offset + 4];
  const size = msg[offset] | (msg[offset + 1] << 8) | (msg[offset + 2] << 16) | (msg[offset + 3] << 24);
  return { bulk, size };
}

function messageDescription(msg) {
  const { bulk, size } = readMessage(msg);
  let hash = 0;
  for (let i = 0; i < size; i++) {
    hash = (((hash << 5) - hash) + msg[i]) >>> 0;
  }
  return `Size ${size} Bulk ${bulk} Hash ${hash}`;
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

function doSend(id, buf, delay) {
  try {
    const connection = gConnections[id];
    if (!connection) {
      return;
    }

    if (!connection.open) {
      connection.outgoing.push(buf);
      return;
    }

    maybeLogMessage("SocketSend", id, new Uint8Array(buf), ++connection.numSends, delay);

    const bulk = checkCompleteMessage(buf);
    if (bulk && connection.bulkOpen) {
      connection.bulkSocket.send(buf);
    } else {
      connection.socket.send(buf);
    }
  } catch (e) {
    PostError(`SendError ${e}`);
  }
}

function onOpen(id, bulk) {
  // Messages can now be sent to the socket.
  const connection = gConnections[id];
  if (bulk) {
    connection.bulkOpen = true;
  } else {
    connection.open = true;
    connection.outgoing.forEach(buf => doSend(id, buf));
    connection.outgoing.length = 0;
  }
}

function onClose(id, bulk) {
  doLog(`ReplayerDisconnected ${id} ${elapsedTime()}\n`);
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

function extractCompleteMessages(id, bulk, data, time) {
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

    maybeLogMessage("SocketRecv", id, msg, ++gConnections[id].numRecvs);

    messages.push(msg.buffer);
    offset += info.size;
  }
  return messages;
}

function onMessage(id, evt, bulk) {
  const connection = gConnections[id];

  // When we have heard back from the replayer, we are fully connected to it.
  if (!connection.connected && !bulk) {
    connection.connected = true;
    doLog(`ReplayerConnected ${id} ${elapsedTime()}\n`);
    postMessage({ kind: "connected", id });
  }

  const messages = extractCompleteMessages(id, bulk, new Uint8Array(evt.data));
  messages.forEach(msg => ChromeUtils.recordReplayOnMessage(id, msg));
}

function onError(id, evt) {
  PostError("SocketError", id);
}

function PostError(why, id) {
  doLog(`ReplayerConnectionError ${id} ${why.toString()} ${elapsedTime()}\n`);
  postMessage({ kind: "error", why: why.toString(), id });
}

function doLog(text) {
  if (gConnected) {
    sendMessageToCloudServer({ kind: "log", text });
  } else {
    postMessage({ kind: "logOffline", text });
  }
}

function elapsedTime() {
  return roundTime(ChromeUtils.recordReplayElapsedTime());
}

function roundTime(time) {
  return ((time * 1000) | 0) / 1000;
}
