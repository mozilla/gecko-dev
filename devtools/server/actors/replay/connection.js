/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { setTimeout } = Components.utils.import(
  "resource://gre/modules/Timer.jsm"
);

const { EventEmitter } = ChromeUtils.import("resource://gre/modules/EventEmitter.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
});

let updateStatusCallback = null;
let connectionStatus = "cloudConnecting.label";

function setConnectionStatusChangeCallback(callback) {
  updateStatusCallback = callback;
}

function getConnectionStatus() {
  return connectionStatus;
}

const gWorker = new Worker("connection-worker.js");
gWorker.addEventListener("message", (evt) => {
  try {
    onMessage(evt);
  } catch (e) {
    ChromeUtils.recordReplayLog(`RecordReplaySocketError ${e} ${e.stack}`);
  }
});

let address = Services.prefs.getStringPref(
  "devtools.recordreplay.cloudServer"
);

const override = getenv("RECORD_REPLAY_SERVER");
if (override) {
  address = override;
}

const gMainChannelId = 1;
gWorker.postMessage({ kind: "openChannel", id: gMainChannelId, address });

function onMessage(evt) {
  switch (evt.data.kind) {
    case "updateStatus":
      connectionStatus = evt.data.status;
      if (updateStatusCallback) {
        updateStatusCallback(connectionStatus);
      }
      break;
    case "commandResult":
      onCommandResult(evt.data.id, evt.data.result);
      break;
  }
}

function getenv(name) {
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  return env.get(name);
}

// Map recording process ID to information about its upload progress.
const gRecordings = new Map();

let gNextMessageId = 1;

function sendCommand(method, params = {}) {
  const id = gNextMessageId++;
  gWorker.postMessage({
    kind: "sendCommand",
    id: gMainChannelId,
    command: { id, method, params },
  });
  return waitForCommandResult(id);
}

// Resolve hooks for promises waiting on a recording to be created.
const gRecordingCreateWaiters = [];

function hashString(str) {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    hash = ((hash << 5) - hash + str.charCodeAt(i)) | 0;
  }
  return hash;
}

function getResourceInfo(url, text) {
  return {
    url,
    checksum: hashString(text).toString(),
  };
}

function isAuthenticationEnabled() {
  // Authentication is controlled by a preference but can be disabled by an
  // environment variable.
  return (
    Services.prefs.getBoolPref(
      "devtools.recordreplay.authentication-enabled"
    ) && !getenv("RECORD_REPLAY_DISABLE_AUTHENTICATION")
  );
}

function isRunningTest() {
  return !!getenv("RECORD_REPLAY_TEST_SCRIPT");
}

async function addRecordingResource(recordingId, url) {
  try {
    const response = await fetch(url);
    if (response.status < 200 || response.status >= 300) {
      console.error("Error fetching recording resource", url, response);
      return null;
    }
    const text = await response.text();

    // If the sourcemap is a data: url, there is no reason for us to upload it as
    // an explicit resource because the URL will be parsed for the content.
    if (url.startsWith("data:")) {
      return text;
    }

    const resource = getResourceInfo(url, text);

    const { known } = await sendCommand("Internal.hasResource", { resource });
    if (!known) {
      await sendCommand("Internal.addResource", { resource, contents: text });
    }

    await sendCommand("Internal.addRecordingResource", {
      recordingId,
      resource,
    });

    return text;
  } catch (e) {
    console.error("Exception fetching recording resource", url, e);
    return null;
  }
}

const SEEN_MANAGERS = new WeakSet();
class Recording extends EventEmitter {
  constructor(pmm) {
    super();
    if (SEEN_MANAGERS.has(pmm)) {
      console.error("Duplicate recording for same child process manager");
    }
    SEEN_MANAGERS.add(pmm);

    this._pmm = pmm;
    this._resourceUploads = [];

    this._pmm.addMessageListener("RecordReplayGeneratedSourceWithSourceMap", {
      receiveMessage: msg => this._onNewSourcemap(msg.data),
    });
    this._pmm.addMessageListener("RecordingFinished", {
      receiveMessage: msg => this._onFinished(msg.data),
    });
    this._pmm.addMessageListener("RecordingUnusable", {
      receiveMessage: msg => this._onUnusable(msg.data),
    });
  }

  get osPid() {
    return this._pmm.osPid;
  }

  _onNewSourcemap({ recordingId, url, sourceMapURL }) {
    this._resourceUploads.push(uploadAllSourcemapAssets(recordingId, url, sourceMapURL));
  }

  async _onFinished(data) {
    this.emit("finished");

    await Promise.all([
      sendCommand("Internal.setRecordingMetadata", {
        authId: getLoggedInUserAuthId(),
        recordingData: data,
      }),

      // Ensure that all sourcemap resources have been sent to the server before
      // we consider the recording saved, so that we don't risk creating a
      // recording session without all the maps available.
      // NOTE: Since we only do this here, recordings that become unusable
      // will never be cleaned up and will leak. We don't currently have
      // an easy way to know the ID of unusable recordings, so we accept
      // the leak as a minor issue.
      Promise.allSettled(this._resourceUploads),
    ]);

    this.emit("saved", data);
  }

  _onUnusable(data) {
    this.emit("unusable", data);
  }
}

async function uploadAllSourcemapAssets(recordingId, url, sourceMapURL) {
  let resolvedSourceMapURL;
  try {
    resolvedSourceMapURL = new URL(sourceMapURL, url).href;
  } catch (e) {
    resolvedSourceMapURL = sourceMapURL;
  }

  const text = await addRecordingResource(recordingId, resolvedSourceMapURL);
  if (text) {
    // Look for sources which are not inlined into the map, and add them as
    // additional recording resources.
    const { sources = [], sourcesContent = [], sourceRoot = "" } = JSON.parse(
      text
    );
    await Promise.all(Array.from({ length: sources.length }, async (_, i) => {
      if (!sourcesContent[i]) {
        const sourceURL = computeSourceURL(url, sourceRoot, sources[i]);
        await addRecordingResource(recordingId, sourceURL);
      }
    }));
  }
}

function getLoggedInUserAuthId() {
  if (isRunningTest()) {
    return "auth0|5f6e41315c863800757cdf74";
  }

  const userPref = Services.prefs.getStringPref("devtools.recordreplay.user");
  if (userPref == "") {
    return;
  }

  const user = JSON.parse(userPref);
  return user == "" ? "" : user.sub;
}

function computeSourceURL(url, root, path) {
  if (root != "") {
    path = root + (root.endsWith("/") ? "" : "/") + path;
  }
  return new URL(path, url).href;
}

const gResultWaiters = new Map();

function waitForCommandResult(id) {
  return new Promise((resolve) => gResultWaiters.set(id, resolve));
}

function onCommandResult(id, result) {
  if (gResultWaiters.has(id)) {
    gResultWaiters.get(id)(result);
    gResultWaiters.delete(id);
  }
}

Services.ppmm.addMessageListener("RecordingStarting", {
  receiveMessage(msg) {
    Services.obs.notifyObservers(new Recording(msg.target), "recordreplay-recording-started");
  },
});

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["setConnectionStatusChangeCallback", "getConnectionStatus"];
