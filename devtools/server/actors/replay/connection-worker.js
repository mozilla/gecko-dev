/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

let gServerSocket;
const gConnections = [];

let gConfig;

ChromeUtils.recordReplayRegisterConnectionWorker(doSend);

self.addEventListener("message", makeInfallible(onMainThreadMessage));

function onMainThreadMessage({ data }) {
  switch (data.kind) {
    case "initialize":
      gConfig = data;
      openServerSocket();
      break;
    case "connect":
      doConnect(data.id, data.channelId);
      break;
    case "log":
      doLog(data.text);
      break;
    case "memoryUsage":
      doMemoryUsage(data.logId, data.total, data.incomplete);
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

// Whether log messages can be sent to the cloud server.
let gConnected = false;

function updateStatus(status) {
  postMessage({ kind: "updateStatus", status });
}

function sendMessageToCloudServer(msg) {
  gServerSocket.send(JSON.stringify(msg));
}

function onServerOpen(evt) {
  sendMessageToCloudServer({ kind: "initialize", buildId: gConfig.buildId });
  updateStatus("cloudInitialize.label");
}

function onServerClose() {
  gConnected = false;
  updateStatus("cloudReconnecting.label");
  setTimeout(openServerSocket, 3000);
  writeLog(`CloudServer Connection Closed`);
}

function onServerError(evt) {
  gConnected = false;
  updateStatus("cloudError.label");
  writeLog(`CloudServer Connection Error`);
}

let gSessionId;

async function onServerMessage(evt) {
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
}

const MessageLogCount = 20;

function isVerbose() {
  return gConfig.env.WEBREPLAY_VERBOSE;
}

function maybeLogMessage(prefix, id, message, count, delay) {
  if (isVerbose() || count <= MessageLogCount) {
    const desc = messageDescription(message);
    const delayText = delay ? ` Delay ${roundTime(delay)}` : "";
    writeLog(`${prefix} Connection ${id} Message ${desc}${delayText}`);
  }
  if (!isVerbose() && count == MessageLogCount) {
    writeLog(`Verbose not set, not logging future ${prefix} messages for connection ${id}`);
  }
  if (delay && delay >= 0.05) {
    writeLog(`Error: Long delay dispatching message: ${delay}`);
  }
}

// How much time to give a new connection to establish itself before closing it
// and reattempting to connect.
const SocketTimeoutMs = 60 * 1000;

// Information about a socket opened for a replayer connection.
function ConnectionSocket(id, bulk) {
  // Identifying information for the socket.
  this.id = id;
  this.bulk = bulk;

  // Underlying websocket.
  this.socket = null;

  // Whether the socket is open, and messages can be sent.
  this.open = false;

  // Whether a message has been received from the other side.
  this.connected = false;

  // Messages queued up for sending before the socket opened.
  this.pending = [];

  // Any data received that does not contain a complete message.
  this.partialMessage = null;

  // How many messages have been sent/received over this connection.
  this.numSends = 0;
  this.numRecvs = 0;
}

ConnectionSocket.prototype = {
  connect(address) {
    if (this.socket) {
      throw new Error("duplicate connect");
    }

    this.socket = new WebSocket(address);
    this.socket.binaryType = "arraybuffer";
    this.socket.onopen = makeInfallible(this.onOpen, this);
    this.socket.onclose = makeInfallible(this.onClose, this);
    this.socket.onmessage = makeInfallible(this.onMessage, this);
    this.socket.onerror = makeInfallible(this.onError, this);
  },

  close() {
    this.socket.close();
  },

  onOpen() {
    writeLog(`ReplayerConnected ${this.id} ${this.bulk}`);
    this.open = true;
    this.pending.forEach(buf => doSend(this.id, buf));
    this.pending.length = 0;
  },

  onClose() {
    writeLog(`ReplayerDisconnected ${this.id} ${this.bulk}`);
    postMessage({ kind: "disconnected", id: this.id });
    gConnections[this.id] = null;
  },

  sendMessage(buf, delay) {
    if (!this.open) {
      this.pending.push(buf);
      return;
    }

    maybeLogMessage("SocketSend", this.id, new Uint8Array(buf), ++this.numSends, delay);

    this.socket.send(buf);
  },

  onMessage(evt) {
    // When we have heard back from the replayer, we are fully connected to it.
    if (!this.connected && !this.bulk) {
      writeLog(`ReplayerEstablished ${this.id}`);
      postMessage({ kind: "connected", id: this.id });
    }
    this.connected = true;

    let data = new Uint8Array(evt.data);

    if (this.partialMessage) {
      const newData = new Uint8Array(this.partialMessage.length + data.length);
      newData.set(this.partialMessage);
      newData.set(data, this.partialMessage.length);
      data = newData;
      this.partialMessage = null;
    }

    // Process any complete messages found in the received data.
    let offset = 0;
    while (true) {
      const info = readMessage(data, offset);
      if (!info || offset + info.size > data.length) {
        if (offset < data.length) {
          this.partialMessage = new Uint8Array(data.buffer, offset);
        }
        break;
      }

      // Copy the message into its own ArrayBuffer.
      const msg = new Uint8Array(info.size);
      msg.set(new Uint8Array(data.buffer, offset, info.size));

      maybeLogMessage("SocketRecv", this.id, msg, ++this.numRecvs);
      ChromeUtils.recordReplayOnMessage(this.id, msg.buffer);

      offset += info.size;
    }
  },

  onError() {
    postError("SocketError", this.id);
  },
};

async function doConnect(id, channelId) {
  writeLog(`StartConnectingToReplayer ${id}`);

  if (gConnections[id]) {
    postError(`Duplicate connection ID ${id}`);
  }
  const connection = {
    // ID assigned to this session by the dispatcher.
    replayerSessionId: null,

    // Sockets used by this connection.
    socket: new ConnectionSocket(id, false),
    bulkSocket: new ConnectionSocket(id, true),

    // Resolve hook for any promise waiting on the socket to connect.
    connectWaiter: null,
  };
  gConnections[id] = connection;

  sendMessageToCloudServer({ kind: "connect", id });

  const address = await new Promise(resolve => (connection.connectWaiter = resolve));

  if (!/^wss?:\/\//.test(address)) {
    postError(`Invalid websocket address ${address}`);
  }

  function urlParams(bulk) {
    const id = connection.replayerSessionId;
    return `id=${id}&dispatchId=${gSessionId}&bulk=${bulk}&env=${JSON.stringify(gConfig.env)}`;
  }

  connection.socket.connect(`${address}/connect?${urlParams(false)}`);
  connection.bulkSocket.connect(`${address}/connect?${urlParams(true)}`);

  if (!gConfig.env.WEBREPLAY_NO_TIMEOUT) {
    setTimeout(() => {
      if (!connection.socket.connected) {
        writeLog(`ReplayerConnectionTimedOut ${id}`);
        connection.socket.close();
        connection.bulkSocket.close();
      }
    }, SocketTimeoutMs);
  }
}

function readMessage(msg, offset = 0) {
  if (offset + 8 > msg.length) {
    return null;
  }
  const bulk = !!msg[offset + 4];
  const kind = msg[offset + 6] | (msg[offset + 7] << 8);
  const size = msg[offset] | (msg[offset + 1] << 8) | (msg[offset + 2] << 16) | (msg[offset + 3] << 24);
  return { bulk, kind, size };
}

function messageDescription(msg) {
  const { bulk, kind, size } = readMessage(msg);
  let hash = 0;
  for (let i = 0; i < size; i++) {
    hash = (((hash << 5) - hash) + msg[i]) >>> 0;
  }
  return `Size ${size} Kind ${kind} Bulk ${bulk} Hash ${hash}`;
}

function checkCompleteMessage(buf) {
  const info = readMessage(new Uint8Array(buf));
  if (!info) {
    postError(`Message too short`);
  }
  if (info.size != buf.byteLength) {
    postError(`Message not complete`);
  }
  return info.bulk;
}

function doSend(id, buf, delay) {
  try {
    const connection = gConnections[id];
    if (!connection) {
      return;
    }

    const bulk = checkCompleteMessage(buf);
    const socket = bulk ? connection.bulkSocket : connection.socket;
    socket.sendMessage(buf, delay);
  } catch (e) {
    postError(`SendError ${e}`);
  }
}

function arrayToString(arr) {
  let str = "";
  for (const n of arr) {
    str += String.fromCharCode(n);
  }
  return escape(str);
}

function postError(why, id) {
  let text;
  try {
    text = why.toString();
  } catch (e) {
    text = "<Unknown>";
  }
  writeLog(`ReplayerConnectionError ${id} ${text}`);
  postMessage({ kind: "error", why: text, id });
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

function writeLog(text) {
  const elapsed = roundTime(ChromeUtils.recordReplayElapsedTime());
  doLog(`[Connection ${elapsed}] ${text}\n`);
}

function doLog(text) {
  if (gConnected) {
    sendMessageToCloudServer({ kind: "log", text });
  } else {
    postMessage({ kind: "logOffline", text });
  }
}

function roundTime(time) {
  return ((time * 1000) | 0) / 1000;
}

function doMemoryUsage(logId, total, incomplete) {
  if (gConnected) {
    sendMessageToCloudServer({ kind: "memoryUsage", logId, total, incomplete });
  }
}
