/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
Cu.import("resource:///modules/devtools/gDevTools.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const {AppManager} = require("devtools/webide/app-manager");

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);

  Logs.init();
});

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);

  Logs.uninit();
});

const Logs = {
  init: function () {
    this.list = document.getElementById("logs");

    Logs.onAppManagerUpdate = Logs.onAppManagerUpdate.bind(this);
    AppManager.on("app-manager-update", Logs.onAppManagerUpdate);

    document.getElementById("close").onclick = Logs.close.bind(this);
  },

  uninit: function () {
    AppManager.off("app-manager-update", Logs.onAppManagerUpdate);
  },

  onAppManagerUpdate: function(event, what, details) {
    switch (what) {
      case "pre-package":
        this.prePackageLog(details);
        break;
    };
  },

  close: function () {
    window.parent.UI.openProject();
  },

  prePackageLog: function (msg, details) {
    if (msg == "start") {
      this.clear();
    } else if (msg == "succeed") {
      setTimeout(function () {
        Logs.close();
      }, 1000);
    } else if (msg == "failed") {
      this.log(details);
    } else {
      this.log(msg);
    }
  },

  clear: function () {
    this.list.innerHTML = "";
  },

  log: function (msg) {
    let line = document.createElement("li");
    line.textContent = msg;
    this.list.appendChild(line);
  }
};
