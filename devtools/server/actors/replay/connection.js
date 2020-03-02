/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
});

// This file provides an interface for connecting middleman processes with
// replaying processes living remotely in the cloud.

// Worker which handles the sockets connecting to remote processes.
let gWorker;

// Callbacks supplied on startup.
let gCallbacks;

// Next ID to use for a replaying process connection.
let gNextConnectionId = 1;

// eslint-disable-next-line no-unused-vars
function Initialize(address, callbacks) {
  gWorker = new Worker("connection-worker.js");
  gWorker.addEventListener("message", evt => {
    try {
      onMessage(evt);
    } catch (e) {
      ChromeUtils.recordReplayLog(`RecordReplaySocketError ${evt.data.why}`);
    }
  });
  gCallbacks = callbacks;

  const buildId = `macOS-${Services.appinfo.appBuildID}`;

  gWorker.postMessage({ kind: "initialize", address, buildId });
}

// ID assigned to this browser session by the cloud server.
let gSessionId;

function onMessage(evt) {
  switch (evt.data.kind) {
    case "updateStatus":
      gCallbacks.updateStatus(evt.data.status);
      if (!evt.data.status.length) {
        flushOfflineLog();
      }
      break;
    case "loaded": {
      const { sessionId, controlJS, replayJS } = evt.data;
      gSessionId = sessionId;
      gCallbacks.loadedJS(sessionId, controlJS, replayJS);
      break;
    }
    case "message":
      gCallbacks.onMessage(evt.data.id, evt.data.buf);
      break;
    case "connectionFailed":
      Services.cpmm.sendAsyncMessage("RecordReplayCriticalError", { kind: "CloudSpawnError" });
      break;
    case "downloadUpdate":
      downloadUpdate(evt.data.updateNeeded);
      break;
    case "connected":
      ChromeUtils.recordReplayLog(`RecordReplayConnected ${gSessionId}`);
      gCallbacks.onConnected(evt.data.id);
      ChromeUtils.recordReplayLog(`RecordReplayConnected ${gSessionId}`);
      break;
    case "disconnected":
      gCallbacks.onDisconnected(evt.data.id);
      ChromeUtils.recordReplayLog(`RecordReplayDisconnected ${gSessionId}`);
      break;
    case "error":
      if (evt.data.id) {
        gCallbacks.onDisconnected(evt.data.id);
      }
      ChromeUtils.recordReplayLog(`RecordReplaySocketError ${gSessionId} ${evt.data.why}`);
      break;
    case "logOffline":
      addToOfflineLog(evt.data.text);
      break;
  }
}

// eslint-disable-next-line no-unused-vars
function Connect(channelId) {
  const id = gNextConnectionId++;
  gWorker.postMessage({ kind: "connect", id, channelId });
  return id;
}

// eslint-disable-next-line no-unused-vars
function SendMessage(id, buf) {
  gWorker.postMessage({ kind: "send", id, buf });
}

// eslint-disable-next-line no-unused-vars
function AddToLog(text) {
  gWorker.postMessage({ kind: "log", text });
}

let gAppUpdater;

function downloadStatusListener(status, ...args) {
  switch (status) {
    case AppUpdater.STATUS.READY_FOR_RESTART:
      gCallbacks.updateStatus("cloudUpdateDownloaded.label");
      break;
    case AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES:
    case AppUpdater.STATUS.CHECKING:
    case AppUpdater.STATUS.STAGING:
      gCallbacks.updateStatus("cloudUpdateDownloading.label");
      break;
    case AppUpdater.STATUS.DOWNLOADING:
      if (!args.length) {
        gCallbacks.updateStatus("cloudUpdateDownloading.label",
                                0, gAppUpdater.update.selectedPatch.size);
      } else {
        const [progress, max] = args;
        gCallbacks.updateStatus("cloudUpdateDownloading.label", progress, max);
      }
      break;
    case AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY:
    case AppUpdater.STATUS.NO_UPDATES_FOUND:
    case AppUpdater.STATUS.UNSUPPORTED_SYSTEM:
    case AppUpdater.STATUS.MANUAL_UPDATE:
    case AppUpdater.STATUS.DOWNLOAD_AND_INSTALL:
    case AppUpdater.STATUS.DOWNLOAD_FAILED:
      gCallbacks.updateStatus("cloudUpdateManualDownload.label");
      break;
  }
}

function downloadUpdate(updateNeeded) {
  // Allow connecting to the cloud with an unknown build.
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  if (env.get("WEBREPLAY_NO_UPDATE")) {
    gCallbacks.updateStatus("");
    return;
  }

  if (gAppUpdater) {
    return;
  }
  gAppUpdater = new AppUpdater();
  if (updateNeeded) {
    gAppUpdater.addListener(downloadStatusListener);
  }
  gAppUpdater.check();
}

function offlineLogPath() {
  let dir = Services.dirsvc.get("UAppData", Ci.nsIFile);
  dir.append("Recordings");

  if (!dir.exists()) {
    OS.File.makeDir(dir.path);
  }

  dir.append("offlineLog.txt");
  return dir.path;
}

// If defined, this reflects the full contents of the offline log.
let offlineLogContents;
let hasOfflineLogFlushTimer;

async function waitForOfflineLogContents() {
  if (offlineLogContents !== undefined) {
    return;
  }

  const path = offlineLogPath();

  if (!(await OS.File.exists(path))) {
    offlineLogContents = "";
    return;
  }

  const file = await OS.File.read(path);
  offlineLogContents = new TextDecoder("utf-8").decode(file);
}

async function addToOfflineLog(text) {
  await waitForOfflineLogContents();
  offlineLogContents += text;

  if (!hasOfflineLogFlushTimer) {
    hasOfflineLogFlushTimer = true;
    setTimeout(() => {
      OS.File.writeAtomic(offlineLogPath(), offlineLogContents);
      hasOfflineLogFlushTimer = false;
    }, 500);
  }
}

async function flushOfflineLog() {
  await waitForOfflineLogContents();

  if (offlineLogContents.length) {
    OS.File.writeAtomic(offlineLogPath(), "");
    AddToLog(offlineLogContents);
  }
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["Initialize", "Connect", "SendMessage", "AddToLog"];
