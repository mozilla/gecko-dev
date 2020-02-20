/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
});

// This file provides an interface for connecting middleman processes with
// replaying processes living remotely in the cloud.

let gCloudAddress;
let gWorker;
let gStatusCallback;
let gLoadedCallback;
let gMessageCallback;
let gNextConnectionId = 1;

// eslint-disable-next-line no-unused-vars
function Initialize(address, statusCallback, loadedCallback, messageCallback) {
  gWorker = new Worker("connection-worker.js");
  gWorker.addEventListener("message", onMessage);
  gStatusCallback = statusCallback;
  gLoadedCallback = loadedCallback;
  gMessageCallback = messageCallback;

  const buildId = `macOS-${Services.appinfo.appBuildID}`;

  gWorker.postMessage({ type: "initialize", address, buildId });
}

function onMessage(evt) {
  switch (evt.data.kind) {
    case "updateStatus":
      gStatusCallback(evt.data.status);
      break;
    case "loaded":
      gLoadedCallback(evt.data.controlJS, evt.data.replayJS);
      break;
    case "message":
      gMessageCallback(evt.data.id, evt.data.buf);
      break;
    case "connectionFailed":
      Services.cpmm.sendAsyncMessage("RecordReplayCriticalError", { kind: "CloudSpawnError" });
      break;
    case "downloadUpdate":
      downloadUpdate(evt.data.updateNeeded);
      break;
  }
}

// eslint-disable-next-line no-unused-vars
function Connect(channelId, callback) {
  const id = gNextConnectionId++;
  gWorker.postMessage({ type: "connect", id, channelId });
  return id;
}

// eslint-disable-next-line no-unused-vars
function SendMessage(id, buf) {
  gWorker.postMessage({ type: "send", id, buf });
}

let gAppUpdater;

function downloadStatusListener(status, ...args) {
  switch (status) {
    case AppUpdater.STATUS.READY_FOR_RESTART:
      gStatusCallback("cloudUpdateDownloaded.label");
      break;
    case AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES:
    case AppUpdater.STATUS.CHECKING:
    case AppUpdater.STATUS.STAGING:
      gStatusCallback("cloudUpdateDownloading.label");
      break;
    case AppUpdater.STATUS.DOWNLOADING:
      if (!args.length) {
        gStatusCallback("cloudUpdateDownloading.label",
                        0, gAppUpdater.update.selectedPatch.size);
      } else {
        const [progress, max] = args;
        gStatusCallback("cloudUpdateDownloading.label", progress, max);
      }
      break;
    case AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY:
    case AppUpdater.STATUS.NO_UPDATES_FOUND:
    case AppUpdater.STATUS.UNSUPPORTED_SYSTEM:
    case AppUpdater.STATUS.MANUAL_UPDATE:
    case AppUpdater.STATUS.DOWNLOAD_AND_INSTALL:
    case AppUpdater.STATUS.DOWNLOAD_FAILED:
      gStatusCallback("cloudUpdateManualDownload.label");
      break;
  }
}

function downloadUpdate(updateNeeded) {
  if (gAppUpdater) {
    return;
  }
  gAppUpdater = new AppUpdater();
  if (updateNeeded) {
    gAppUpdater.addListener(downloadStatusListener);
  }
  gAppUpdater.check();
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["Initialize", "Connect", "SendMessage"];
