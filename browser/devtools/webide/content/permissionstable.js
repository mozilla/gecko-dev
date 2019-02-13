/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const {AppManager} = require("devtools/webide/app-manager");
const {Connection} = require("devtools/client/connection-manager");

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  document.querySelector("#close").onclick = CloseUI;
  AppManager.on("app-manager-update", OnAppManagerUpdate);
  BuildUI();
}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  AppManager.off("app-manager-update", OnAppManagerUpdate);
});

function CloseUI() {
  window.parent.UI.openProject();
}

function OnAppManagerUpdate(event, what) {
  if (what == "connection" || what == "runtime-global-actors") {
    BuildUI();
  }
}

function generateFields(json) {
  let table = document.querySelector("table");
  let permissionsTable = json.rawPermissionsTable;
  for (let name in permissionsTable) {
    let tr = document.createElement("tr");
    tr.className = "line";
    let td = document.createElement("td");
    td.textContent = name;
    tr.appendChild(td);
    for (let type of ["app","privileged","certified"]) {
      let td = document.createElement("td");
      if (permissionsTable[name][type] == json.ALLOW_ACTION) {
        td.textContent = "✓";
        td.className = "permallow";
      }
      if (permissionsTable[name][type] == json.PROMPT_ACTION) {
        td.textContent = "!";
        td.className = "permprompt";
      }
      if (permissionsTable[name][type] == json.DENY_ACTION) {
        td.textContent = "✕";
        td.className = "permdeny"
      }
      tr.appendChild(td);
    }
    table.appendChild(tr);
  }
}

let getRawPermissionsTablePromise; // Used by tests
function BuildUI() {
  let table = document.querySelector("table");
  let lines = table.querySelectorAll(".line");
  for (let line of lines) {
    line.remove();
  }

  if (AppManager.connection &&
      AppManager.connection.status == Connection.Status.CONNECTED &&
      AppManager.deviceFront) {
    getRawPermissionsTablePromise = AppManager.deviceFront.getRawPermissionsTable()
                                    .then(json => generateFields(json));
  } else {
    CloseUI();
  }
}
