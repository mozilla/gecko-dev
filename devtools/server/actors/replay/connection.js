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

XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
});

// This interface connects to the cloud service and manages uploading recording data.

// Worker which handles the sockets connecting to remote processes.
let gWorker;

// Callbacks supplied on startup.
let gCallbacks;

let gConfig;

// When connecting we open an initial channel for commands not associated with a recording.
let gMainChannelId;

// eslint-disable-next-line no-unused-vars
function Initialize(callbacks) {
  gWorker = new Worker("connection-worker.js");
  gWorker.addEventListener("message", (evt) => {
    try {
      onMessage(evt);
    } catch (e) {
      ChromeUtils.recordReplayLog(`RecordReplaySocketError ${e} ${e.stack}`);
    }
  });
  gCallbacks = callbacks;

  let address = Services.prefs.getStringPref(
    "devtools.recordreplay.cloudServer"
  );

  const override = getenv("RECORD_REPLAY_SERVER");
  if (override) {
    address = override;
  }

  // During automated tests, sometimes we want to use different dispatchers
  // depending on the recording URL, e.g. to use a dispatcher on the localhost
  // for the pages being tested but the normal dispatcher for recordings of the
  // devtools viewer itself.
  const altAddress = getenv("RECORD_REPLAY_ALTERNATE_SERVER");
  const altPattern = getenv("RECORD_REPLAY_ALTERNATE_SERVER_PATTERN");

  gConfig = { address, altAddress, altPattern };

  gMainChannelId = openChannel(address);
}

let gNextChannelId = 1;

function openChannel(address) {
  const id = gNextChannelId++;
  gWorker.postMessage({ kind: "openChannel", id, address });
  return id;
}

function onMessage(evt) {
  switch (evt.data.kind) {
    case "updateStatus":
      gCallbacks.updateStatus(evt.data.status);
      if (evt.data.status == "") {
        loadAssertionFilters();
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

async function loadAssertionFilters() {
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  if (
    env.get("RECORD_REPLAY_RECORD_EXECUTION_ASSERTS") ||
    env.get("RECORD_REPLAY_RECORD_JS_ASSERTS")
  ) {
    // Use the values from the current environment.
    return;
  }

  const filters = await sendCommand("Internal.getAssertionFilters");
  if (!filters) {
    return;
  }

  const { execution, values } = filters;

  env.set(
    "RECORD_REPLAY_RECORD_EXECUTION_ASSERTS",
    stringify([...execution, ...values])
  );
  env.set("RECORD_REPLAY_RECORD_JS_ASSERTS", stringify(values));

  function stringify(asserts) {
    let text = "";
    for (const { url, startLine, endLine } of asserts) {
      text += `${url}@${startLine}@${endLine}@`;
    }
    return text;
  }
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

const recordingAsyncOperations = new Map();
function getOrCreateRecordingAsyncOps(recordingId) {
  let ops = recordingAsyncOperations.get(recordingId);
  if (!ops) {
    if (ops === null) {
      console.error(
        "Unexpectedly accessing async operation list after taking it."
      );
    }

    ops = [];
    recordingAsyncOperations.set(recordingId, ops);
  }
  return ops;
}
function takeRecordingAsyncOps(recordingId) {
  let ops = recordingAsyncOperations.get(recordingId);
  // Set to null instead of deleting so we can show a warning if the
  // recording's async operation list is accessed after this point.
  recordingAsyncOperations.set(recordingId, null);
  return ops || [];
}

Services.ppmm.addMessageListener("RecordReplayGeneratedSourceWithSourceMap", {
  receiveMessage(msg) {
    const { recordingId, url, sourceMapURL } = msg.data;

    getOrCreateRecordingAsyncOps(recordingId)
      .push(uploadAllSourcemapAssets(recordingId, url, sourceMapURL));
  },
});

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

Services.ppmm.addMessageListener("RecordingFinished", {
  async receiveMessage(msg) {
    // NOTE(dmiller): this can be null in the devtools tests for some reason, but not in production
    // Not sure why.
    if (isRunningTest() && !msg.data) {
      console.log("got RecordingFinished with empty msg data, skipping");
      return;
    }

    await Promise.all([
      sendCommand("Internal.setRecordingMetadata", {
        authId: getLoggedInUserAuthId(),
        recordingData: msg.data,
      }),

      // Ensure that all sourcemap resources have been sent to the server before
      // we consider the recording saved, so that we don't risk creating a
      // recording session without all the maps available.
      // NOTE: Since we only do this here, recordings that become unusable
      // will never be cleaned up and will leak. We don't currently have
      // an easy way to know the ID of unusable recordings, so we accept
      // the leak as a minor issue.
      Promise.allSettled(takeRecordingAsyncOps(msg.data.id)),
    ]);
    Services.cpmm.sendAsyncMessage("RecordingSaved", msg.data);
  },
});

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

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["Initialize"];
