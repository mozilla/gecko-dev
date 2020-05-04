/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { setTimeout } = Components.utils.import('resource://gre/modules/Timer.jsm');
const { onStartRecording, onFinishedRecording } =
  ChromeUtils.import("resource:///modules/DevToolsStartup.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
});

// This interface connects to the cloud service and manages uploading recording data.

// Worker which handles the sockets connecting to remote processes.
let gWorker;

// Callbacks supplied on startup.
let gCallbacks;

// eslint-disable-next-line no-unused-vars
function Initialize(callbacks) {
  gWorker = new Worker("connection-worker.js");
  gWorker.addEventListener("message", evt => {
    try {
      onMessage(evt);
    } catch (e) {
      ChromeUtils.recordReplayLog(`RecordReplaySocketError ${e} ${e.stack}`);
    }
  });
  gCallbacks = callbacks;

  let address = Services.prefs.getStringPref("devtools.recordreplay.cloudServer");

  const override = getenv("WEBREPLAY_SERVER");
  if (override) {
    address = override;
  }

  const buildId = `macOS-${Services.appinfo.appBuildID}`;
  gWorker.postMessage({ kind: "initialize", address, buildId });
}

function onMessage(evt) {
  switch (evt.data.kind) {
    case "updateStatus":
      gCallbacks.updateStatus(evt.data.status);
      break;
    case "commandResult":
      onCommandResult(evt.data.id, evt.data.result);
      break;
  }
}

function getenv(name) {
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  return env.get(name);
}

// Map recording process ID to information about its upload progress.
const gRecordings = new Map();

let gNextMessageId = 1;

Services.ppmm.addMessageListener("UploadRecordingData", {
  async receiveMessage(msg) {
    const { pid, offset, length, buf, description } = msg.data;

    let first = !gRecordings.has(pid);
    if (first) {
      if (offset != 0) {
        // We expect to get recording data notifications in order. If the offset
        // is non-zero then this is for a recording child that we consider to
        // be destroyed.
        return;
      }

      const buildId = `macOS-${Services.appinfo.appBuildID}`;
      const id = gNextMessageId++;
      gWorker.postMessage({
        kind: "sendUploadCommand",
        pid,
        command: {
          id,
          method: "Internal.createRecording",
          params: { buildId },
        },
      });
      const info = {
        createPromise: waitForCommandResult(id),
        dataPromises: [],
        destroyed: false,
      };
      gRecordings.set(pid, info);
    }
    const info = gRecordings.get(pid);
    const { recordingId } = await info.createPromise;

    if (info.destroyed) {
      return;
    }

    if (first) {
      onStartRecording(recordingId);
    }

    const id = gNextMessageId++;
    gWorker.postMessage({
      kind: "sendUploadCommand",
      pid,
      command: {
        id,
        method: "Internal.addRecordingData",
        params: { recordingId, offset, length },
      },
    });

    gWorker.postMessage({ kind: "sendUploadBinaryData", pid, buf });

    info.dataPromises.push(waitForCommandResult(id));

    if (description) {
      // This is for the last flush before the recording tab is closed,
      // add a recording description.
      const id = gNextMessageId++;
      gWorker.postMessage({
        kind: "sendCommand",
        command: {
          id,
          method: "Internal.addRecordingDescription",
          params: { recordingId, ...description },
        },
      });

      waitForCommandResult(id).then(() => onFinishedRecording(recordingId));

      // Ignore any other flushes from this pid.
      RecordingDestroyed(pid);
    }
  }
});

async function RecordingDestroyed(pid) {
  const info = gRecordings.get(pid);
  if (!info || info.destroyed) {
    return;
  }
  info.destroyed = true;
  gRecordings.delete(pid);

  await Promise.all(info.dataPromises);

  gWorker.postMessage({ kind: "stopUpload", pid });
}

const gResultWaiters = new Map();

function waitForCommandResult(id) {
  return new Promise(resolve => gResultWaiters.set(id, resolve));
}

function onCommandResult(id, result) {
  if (gResultWaiters.has(id)) {
    gResultWaiters.get(id)(result);
    gResultWaiters.delete(id);
  }
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["Initialize", "RecordingDestroyed"];
