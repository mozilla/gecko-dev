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

function replayDir() {
  let dir = Services.dirsvc.get("UAppData", Ci.nsIFile);
  dir.append("Replay");

  if (!dir.exists()) {
    OS.File.makeDir(dir.path);
  }

  return dir;
}

function getRecordingsPath() {
  const dir = replayDir();
  dir.append("recordings.json");
  return dir.path;
}

async function readRecordingsFile() {
  if (!(await OS.File.exists(getRecordingsPath()))) {
    return {};
  }
  const file = await OS.File.read(getRecordingsPath());
  const string = new TextDecoder("utf-8").decode(file);
  return JSON.parse(string);
}

let gLastCopyLink = null;

async function showRecordings() {
  const container = document.querySelector(".recordings-list");
  const template = document.querySelector("#recording-row");

  container.innerHTML = "";

  for (const { url, originalURL, title, time, duration } of gRecordings) {
    const newRow = document.importNode(template.content, true);
    newRow.querySelector(".recording-title").innerText = title;
    newRow.querySelector(".recording-url").innerText = originalURL;
    newRow.querySelector(".recording-start").innerText = formatTime(time);
    newRow.querySelector(".recording-duration").innerText = formatDuration(duration);
    newRow.querySelector(".recording").addEventListener("click", e => {
      document.location = url;
    });
    const copylink = newRow.querySelector(".copylink");
    copylink.addEventListener("click", e => {
      navigator.clipboard.writeText(url);
      copylink.innerText = "Copied!";
      if (gLastCopyLink) {
        gLastCopyLink.innerText = "Copy Link";
      }
      gLastCopyLink = copylink;
    });
    newRow.querySelector(".remove").addEventListener("click", e => {
      gRecordings = gRecordings.filter(item => item.url != url);
      showRecordings();
    });
    container.appendChild(newRow);
  }

  const vis = gRecordings.length ? "hidden" : "visible";
  document.querySelector(".no-recordings").style.visibility = vis;
}

window.onload = async function() {
  gRecordings = await readRecordingsFile();

  gRecordings = [
    {
      url: "replay://85074cf9-d2ca-4958-848d-6c106b854293",
      originalURL: "http://example.com",
      title: "Page Title",
      time: Date.now(),
      duration: 1000 * 10,
    },
    {
      url: "replay://5770642e-5835-4aa7-ab0c-1ad46e4fafa9",
      originalURL: "http://foobar.org",
      title: "Foobar Title",
      time: Date.now() - 1000 * 10000,
      duration: 1000 * 100,
    },
  ];

  showRecordings();
};
