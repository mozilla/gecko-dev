/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { setTimeout } = Components.utils.import('resource://gre/modules/Timer.jsm');
const {
  onFinishedRecording,
} = ChromeUtils.import("resource:///modules/DevToolsStartup.jsm");

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
  gWorker.addEventListener("message", evt => {
    try {
      onMessage(evt);
    } catch (e) {
      ChromeUtils.recordReplayLog(`RecordReplaySocketError ${e} ${e.stack}`);
    }
  });
  gCallbacks = callbacks;

  let address = Services.prefs.getStringPref("devtools.recordreplay.cloudServer");

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
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  return env.get(name);
}

async function loadAssertionFilters() {
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  if (env.get("RECORD_REPLAY_RECORD_EXECUTION_ASSERTS") ||
      env.get("RECORD_REPLAY_RECORD_JS_ASSERTS")) {
    // Use the values from the current environment.
    return;
  }

  const filters = await sendCommand(gMainChannelId, "Internal.getAssertionFilters");
  if (!filters) {
    return;
  }

  const { execution, values } = filters;

  env.set("RECORD_REPLAY_RECORD_EXECUTION_ASSERTS", stringify([...execution, ...values]));
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

function sendCommand(channelId, method, params) {
  const id = gNextMessageId++;
  gWorker.postMessage({
    kind: "sendCommand",
    id: channelId,
    command: { id, method, params },
  });
  return waitForCommandResult(id);
}

// Resolve hooks for promises waiting on a recording to be created.
const gRecordingCreateWaiters = [];

function hashString(str) {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    hash = (((hash << 5) - hash) + str.charCodeAt(i)) | 0;
  }
  return hash;
}

function getResourceInfo(url, text) {
  return {
    url,
    checksum: hashString(text).toString(),
  };
}

async function addRecordingResource(info, recordingId, url) {
  try {
    const response = await fetch(url);
    if (response.status < 200 || response.status >= 300) {
      console.error("Error fetching recording resource", url, response);
      return null;
    }
    const text = await response.text();
    const resource = getResourceInfo(url, text);

    await sendCommand(
      info.messageChannelId,
      "Internal.addRecordingResource",
      { recordingId, resource }
    );

    const { known } = await sendCommand(
      info.messageChannelId,
      "Internal.hasResource",
      { resource }
    );
    if (!known) {
      await sendCommand(
        info.messageChannelId,
        "Internal.addResource",
        { resource, contents: text }
      );
    }

    return text;
  } catch (e) {
    console.error("Exception fetching recording resource", url, e);
    return null;
  }
}

Services.ppmm.addMessageListener("RecordReplayGeneratedSourceWithSourceMap", {
  async receiveMessage(msg) {
    const { pid, url, sourceMapURL } = msg.data;

    // Wait for a recording to be created for this pid, if it hasn't already happened.
    let info;
    while (true) {
      info = gRecordings.get(pid);
      if (info) {
        break;
      }
      await new Promise(resolve => gRecordingCreateWaiters.push(resolve));
    }
    const { recordingId } = await info.createPromise;

    let resolvedSourceMapURL;
    try {
      resolvedSourceMapURL = new URL(sourceMapURL, url).href;
    } catch (e) {
      resolvedSourceMapURL = sourceMapURL;
    }
    const promise = addRecordingResource(info, recordingId, resolvedSourceMapURL);
    info.dataPromises.push(promise);

    const text = await promise;
    if (text) {
      // Look for sources which are not inlined into the map, and add them as
      // additional recording resources.
      const { sources = [], sourcesContent = [], sourceRoot = "" } = JSON.parse(text);
      for (let i = 0; i < sources.length; i++) {
        if (!sourcesContent[i]) {
          const sourceURL = computeSourceURL(url, sourceRoot, sources[i]);
          info.dataPromises.push(addRecordingResource(info, recordingId, sourceURL));
        }
      }
    }
  }
});

function computeSourceURL(url, root, path) {
  if (root != "") {
    path = root + (root.endsWith("/") ? "" : "/") + path;
  }
  return new URL(path, url).href;
}

async function RecordingDestroyed(pid) {
  const info = gRecordings.get(pid);
  if (!info || info.destroyed) {
    return;
  }
  info.destroyed = true;
  gRecordings.delete(pid);

  await Promise.all(info.dataPromises);

  gWorker.postMessage({ kind: "closeChannel", id: info.uploadChannelId });
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
