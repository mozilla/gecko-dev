/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

let gRecordings;

function formatTime(time) {
  return new Intl.DateTimeFormat("default", {
    month: "short",
    day: "numeric",
    hour: "numeric",
    minute: "numeric",
  }).format(new Date(time));
}

const MsPerSecond = 1000;

function formatDuration(duration) {
  return `${(duration / MsPerSecond) | 0} seconds`;
}

// See also DevToolsStartup.jsm
function getRecordingsPath() {
  let dir = Services.dirsvc.get("UAppData", Ci.nsIFile);
  dir.append("Recordings");

  if (!dir.exists()) {
    OS.File.makeDir(dir.path);
  }

  dir.append("recordings.json");
  return dir.path;
}

async function readRecordingsFile() {
  const path = getRecordingsPath();

  if (!(await OS.File.exists(path))) {
    return [];
  }

  const file = await OS.File.read(path);
  return JSON.parse(new TextDecoder("utf-8").decode(file));
}

function updateRecordings(recordings) {
  gRecordings = recordings;
  OS.File.writeAtomic(getRecordingsPath(), JSON.stringify(recordings));
  showRecordings();
}

let gLastCopyLink = null;

async function showRecordings() {
  const container = document.querySelector(".recordings-list");
  const template = document.querySelector("#recording-row");

  container.innerHTML = "";

  for (const { uuid, url, title, date, duration } of gRecordings) {
    const recordingUrl = `about:webreplay?recording=${uuid}`;
    const newRow = document.importNode(template.content, true);
    newRow.querySelector(".recording-title").innerText = title;
    newRow.querySelector(".recording-url").innerText = url;
    newRow.querySelector(".recording-start").innerText = formatTime(date);
    newRow.querySelector(".recording-duration").innerText = formatDuration(duration);
    newRow.querySelector(".recording").addEventListener("click", e => {
      document.location = recordingUrl;
    });
    const copylink = newRow.querySelector(".copylink");
    copylink.addEventListener("click", e => {
      navigator.clipboard.writeText(recordingUrl);
      copylink.innerText = "Copied!";
      if (gLastCopyLink) {
        gLastCopyLink.innerText = "Copy Link";
      }
      gLastCopyLink = copylink;
    });
    newRow.querySelector(".remove").addEventListener("click", e => {
      updateRecordings(gRecordings.filter(item => item.uuid != uuid));
    });
    container.appendChild(newRow);
  }

  document.querySelector(".no-recordings").hidden = !!gRecordings.length;
}

function showError(kind) {
  document.querySelector(".recordings-title").hidden = true;
  document.querySelector(".no-recordings").hidden = true;
  document.querySelector(".error-title").hidden = false;

  document.querySelector(".error-message").setAttribute("data-l10n-id", kind);
}

window.onload = async function() {
  let match;

  if (match = /recording=(.*)/.exec(window.location)) {
    const status = ChromeUtils.getCloudReplayStatus();
    if (status) {
      window.location = "about:webreplay?error=CloudNotConnected";
      return;
    }

    const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
    const tab = gBrowser.selectedTab;
    gBrowser.selectedTab = gBrowser.addWebTab(null, {
      replayExecution: `webreplay://${match[1]}`,
      index: tab._tPos + 1,
    });
    gBrowser.removeTab(tab);

    const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
    const { gDevToolsBrowser } = require("devtools/client/framework/devtools-browser");
    gDevToolsBrowser.toggleToolboxCommand(gBrowser, Cu.now());
    return;
  }

  if (match = /error=(.*)/.exec(window.location)) {
    showError(match[1]);
    return;
  }

  gRecordings = await readRecordingsFile();

  showRecordings();
};
