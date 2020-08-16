/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
  const total = (duration / MsPerSecond) | 0;
  const seconds = total % 60;
  const minutes = Math.floor(total / 60);

  const formattedSeconds = seconds > 9 ? seconds : `0${seconds}`;
  return `${minutes}:${formattedSeconds}`;
}

// Maximum number of characters to print for titles and URLs.
const MaxStringLength = 50;

function formatString(string) {
  if (string.length > MaxStringLength) {
    return string.substring(0, MaxStringLength - 3) + "...";
  }
  return string;
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

function makeRecordingElement(template, desc) {
  const { recordingId, url, title, date, duration, lastScreenData } = desc;
  const recordingUrl = `https://replay.io/view?id=${recordingId}`;
  const newRow = document.importNode(template.content, true);
  newRow.querySelector(".title").innerText = formatString(title);
  // newRow.querySelector(".url").innerText = formatString(url);
  newRow.querySelector(".start").innerText = formatTime(date);
  newRow.querySelector(".duration").innerText = formatDuration(duration);
  newRow.querySelector(".recording").href = recordingUrl;

  if (lastScreenData) {
    const img = document.createElement("img");
    img.src = "data:image/jpeg;base64, " + lastScreenData;
    newRow.querySelector(".screenshot").appendChild(img);
  }

  // screenshot.style.height = "100%";
  // screenshot.style.display = "block";
  // screenshot.style.position = "absolute";
  // screenshot.style.left = "0";

  newRow.querySelector(".copylink")?.addEventListener("click", (e) => {
    e.preventDefault();
    e.stopPropagation();
    e.target.classList.add("copied");
    setTimeout(() => {
      e.target.classList.remove("copied");
    }, 2000);
    navigator.clipboard.writeText(recordingUrl);
  });

  newRow.querySelector(".remove")?.addEventListener("click", (e) => {
    e.preventDefault();
    e.stopPropagation();
    updateRecordings(
      gRecordings.filter((item) => item.recordingId != recordingId)
    );
  });

  return newRow;
}

async function showRecordings() {
  const container = document.querySelector(".recordings-list");
  const template = document.querySelector("#recording-row");

  container.innerHTML = "";
  for (const desc of gRecordings) {
    try {
      const newRow = makeRecordingElement(template, desc);
      container.appendChild(newRow);
    } catch (e) {
      console.error("Couldn't create recording element", e);
    }
  }

  document.querySelector(".no-recordings").hidden = !!gRecordings.length;
}

function showError(kind) {
  document.querySelector(".recordings-title").hidden = true;
  document.querySelector(".no-recordings").hidden = true;
  document.querySelector(".error-title").hidden = false;

  document.querySelector(".error-message").setAttribute("data-l10n-id", kind);
}

window.onload = async function () {
  let match;

  if ((match = /error=(.*)/.exec(window.location))) {
    showError(match[1]);
    return;
  }

  gRecordings = await readRecordingsFile();

  showRecordings();
};
