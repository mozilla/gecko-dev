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

const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);

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
    case "commandResponse":
      onCommandResponse(evt.data.msg);
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

  _onNewSourcemap(params) {
    this._resourceUploads.push(uploadAllSourcemapAssets(params).catch(err => {
      console.error("Exception while processing sourcemap", err, params);
    }));
  }

  async _onFinished(data) {
    this.emit("finished");

    try {
      await sendCommand("Internal.setRecordingMetadata", {
        authId: getLoggedInUserAuthId(),
        recordingData: data,
      });
    } catch (err) {
      console.error("Exception while setting recording metadata", err);
      let message;
      if (err instanceof CommandError) {
        message = ": " + err.message;
      }
      this._onUnusable({ why: "failed to set recording metadata" + message });
      return;
    }

    // Ensure that all sourcemap resources have been sent to the server before
    // we consider the recording saved, so that we don't risk creating a
    // recording session without all the maps available.
    await Promise.all(this._resourceUploads);

    this.emit("saved", data);
  }

  _onUnusable(data) {
    this.emit("unusable", data);
  }
}

function uploadSourceMap(
  recordingId,
  mapText,
  baseURL,
  { targetContentHash, targetURLHash, targetMapURLHash }
) {
  return withUploadedResource(mapText, async (resource) => {
    const result = await sendCommand("Recording.addSourceMap", {
      recordingId,
      resource,
      baseURL,
      targetContentHash,
      targetURLHash,
      targetMapURLHash,
    })
    return result.id;
  });
}

async function uploadAllSourcemapAssets({
  recordingId,
  targetURLHash,
  targetContentHash,
  targetMapURLHash,
  sourceMapURL,
  sourceMapBaseURL
}) {
  const result = await fetchText(sourceMapURL);
  if (!result) {
    return;
  }
  const mapText = result.text;

  const { sources } =
    collectUnresolvedSourceMapResources(mapText, sourceMapURL, sourceMapBaseURL);

  let mapUploadFailed = false;
  let mapIdPromise;
  function ensureMapUploading() {
    if (!mapIdPromise) {
      mapIdPromise = uploadSourceMap(recordingId, mapText, sourceMapBaseURL, {
        targetContentHash,
        targetURLHash,
        targetMapURLHash
      });
      mapIdPromise.catch(() => {
        mapUploadFailed = true;
      });
    }
    return mapIdPromise;
  }

  await Promise.all([
    // For data URLs, we don't want to start uploading the map by default
    // because for most data: URLs, the inline sources will contain
    // everything needed for debugging, and the server can resolve
    // data: URLs itself without needing resources to be uploaded.
    // If the data: map _does_ need to be uploaded, that will be handled
    // once that is detected by the sources.
    sourceMapURL.startsWith("data:") ? undefined : ensureMapUploading(),
    Promise.all(sources.map(async ({ offset, url }) => {
      const result = await fetchText(url);
      if (!result || mapUploadFailed) {
        return;
      }

      await Promise.all([
        // Once we know there are original sources that we can upload, we want
        // ensure that the map is uploading, if it wasn't already.
        ensureMapUploading(),
        withUploadedResource(result.text, async (resource) => {
          let parentId;
          try {
            parentId = await ensureMapUploading();
          } catch (err) {
            // The error will be handled above, but if it fails,
            // that we don't bother seeing the failure that should
            // trigger a retry of this.
            return;
          }

          await sendCommand("Recording.addOriginalSource", {
            recordingId,
            resource,
            parentId,
            parentOffset: offset,
          });
        })
      ]);
    })),
  ]);
}

function collectUnresolvedSourceMapResources(mapText, mapURL, mapBaseURL) {
  let obj;
  try {
    obj = JSON.parse(mapText);
    if (typeof obj !== "object" || !obj) {
      return {
        sources: [],
      };
    }
  } catch (err) {
    console.error("Exception parsing sourcemap JSON", mapURL);
    return {
      sources: [],
    };
  }

  function logError(msg) {
    console.error(msg, mapURL, map, sourceOffset, sectionOffset);
  }

  const unresolvedSources = [];
  let sourceOffset = 0;

  if (obj.version !== 3) {
    logError("Invalid sourcemap version");
    return;
  }

  if (obj.sources != null) {
    const { sourceRoot, sources, sourcesContent } = obj;

    if (Array.isArray(sources)) {
      for (let i = 0; i < sources.length; i++) {
        const offset = sourceOffset++;

        if (
          !Array.isArray(sourcesContent) ||
          typeof sourcesContent[i] !== "string"
        ) {
          let url = sources[i];
          if (typeof sourceRoot === "string" && sourceRoot) {
            url = sourceRoot.replace(/\/?/, "/") + url;
          }
          let sourceURL;
          try {
            sourceURL = new URL(url, mapBaseURL).toString();
          } catch {
            logError("Unable to compute original source URL: " + url);
            continue;
          }

          unresolvedSources.push({
            offset,
            url: sourceURL,
          });
        }
      }
    } else {
      logError("Invalid sourcemap source list");
    }
  }

  return {
    sources: unresolvedSources,
  };
}

async function fetchText(url) {
  try {
    const response = await fetch(url);
    if (response.status < 200 || response.status >= 300) {
      console.error("Error fetching recording resource", url, response);
      return null;
    }

    return {
      url,
      text: await response.text(),
    };
  } catch (e) {
    console.error("Exception fetching recording resource", url, e);
    return null;
  }
}

async function uploadResource(text) {
  const hash = "sha256:" + CryptoUtils.sha256(text);
  const { token } = await sendCommand("Resource.token", { hash });
  let resource = {
    token,
    saltedHash: "sha256:" + CryptoUtils.sha256(token + text)
  };

  const { exists } = await sendCommand("Resource.exists", { resource });
  if (!exists) {
    ({ resource } = await sendCommand("Resource.create", { content: text }));
  }
  return resource;
}

const RETRY_COUNT = 3;

async function withUploadedResource(text, callback) {
  for (let i = 0; i < RETRY_COUNT - 1; i++) {
    try {
      return await callback(await uploadResource(text));
    } catch (err) {
      // If the connection dies, we want to retry, and if it died and
      // reconnected while something else was going on, the token will
      // likely have been invalidated, so we want to retry in that case too.
      if (err instanceof CommandError && (err.code === -1 || err.code === 39) ) {
        console.error("Resource Upload failed, retrying", err);
        continue;
      }
      throw err;
    }
  }

  return callback(await uploadResource(text));
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

const gResultWaiters = new Map();

function waitForCommandResult(id) {
  return new Promise((resolve, reject) => gResultWaiters.set(id, { resolve, reject }));
}

function onCommandResponse(msg) {
  const { id } = msg;
  if (gResultWaiters.has(id)) {
    const { resolve, reject } = gResultWaiters.get(id);
    gResultWaiters.delete(id);

    if (msg.error) {
      reject(new CommandError(msg.error.message, msg.error.code));
    } else {
      resolve(msg.result);
    }
  }
}

class CommandError extends Error {
  constructor(message, code) {
    super(message);
    this.code = code;
  }
}

Services.ppmm.addMessageListener("RecordingStarting", {
  receiveMessage(msg) {
    Services.obs.notifyObservers(new Recording(msg.target), "recordreplay-recording-started");
  },
});

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["setConnectionStatusChangeCallback", "getConnectionStatus"];
