// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

var gView = null;

function showView(aView) {
  gView = aView;

  gView.show();

  // If the view's show method immediately showed a different view then don't
  // do anything else
  if (gView != aView)
    return;

  let viewNode = document.getElementById(gView.nodeID);
  viewNode.parentNode.selectedPanel = viewNode;

  // For testing dispatch an event when the view changes
  var event = document.createEvent("Events");
  event.initEvent("ViewChanged", true, true);
  viewNode.dispatchEvent(event);
}

function showButtons(aCancel, aBack, aNext, aDone) {
  document.getElementById("cancel").hidden = !aCancel;
  document.getElementById("back").hidden = !aBack;
  document.getElementById("next").hidden = !aNext;
  document.getElementById("done").hidden = !aDone;
}

function isAddonDistroInstalled(aID) {
  let branch = Services.prefs.getBranch("extensions.installedDistroAddon.");
  if (!branch.prefHasUserValue(aID))
    return false;

  return branch.getBoolPref(aID);
}

function orderForScope(aScope) {
  return aScope == AddonManager.SCOPE_PROFILE ? 1 : 0;
}

var gAddons = {};

var gChecking = {
  nodeID: "checking",

  _progress: null,
  _addonCount: 0,
  _completeCount: 0,

  show: function gChecking_show() {
    showButtons(true, false, false, false);
    this._progress = document.getElementById("checking-progress");

    AddonManager.getAllAddons(aAddons => {
      if (aAddons.length == 0) {
        window.close();
        return;
      }

      aAddons = aAddons.filter(function gChecking_filterAddons(aAddon) {
        if (aAddon.id == AddonManager.hotfixID) {
          return false;
        }
        if (aAddon.type == "plugin" || aAddon.type == "service")
          return false;

        if (aAddon.type == "theme") {
          // Don't show application shipped themes
          if (aAddon.scope == AddonManager.SCOPE_APPLICATION)
            return false;
          // Don't show already disabled themes
          if (aAddon.userDisabled)
            return false;
        }

        return true;
      });

      this._addonCount = aAddons.length;
      this._progress.value = 0;
      this._progress.max = aAddons.length;
      this._progress.mode = "determined";

      AddonRepository.repopulateCache().then(() => {
        for (let addonItem of aAddons) {
          // Ignore disabled themes
          if (addonItem.type != "theme" || !addonItem.userDisabled) {
            gAddons[addonItem.id] = {
              addon: addonItem,
              install: null,
              wasActive: addonItem.isActive
            }
          }

          addonItem.findUpdates(this, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
        }
      });
    });
  },

  onUpdateAvailable: function gChecking_onUpdateAvailable(aAddon, aInstall) {
    // If the add-on can be upgraded then remember the new version
    if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE)
      gAddons[aAddon.id].install = aInstall;
  },

  onUpdateFinished: function gChecking_onUpdateFinished(aAddon, aError) {
    this._completeCount++;
    this._progress.value = this._completeCount;

    if (this._completeCount < this._addonCount)
      return;

    var addons = [gAddons[id] for (id in gAddons)];

    addons.sort(function sortAddons(a, b) {
      let orderA = orderForScope(a.addon.scope);
      let orderB = orderForScope(b.addon.scope);

      if (orderA != orderB)
        return orderA - orderB;

      return String.localeCompare(a.addon.name, b.addon.name);
    });

    let rows = document.getElementById("select-rows");
    let lastAddon = null;
    for (let entry of addons) {
      if (lastAddon &&
          orderForScope(entry.addon.scope) != orderForScope(lastAddon.scope)) {
        let separator = document.createElement("separator");
        rows.appendChild(separator);
      }

      let row = document.createElement("row");
      row.setAttribute("id", entry.addon.id);
      row.setAttribute("class", "addon");
      rows.appendChild(row);
      row.setAddon(entry.addon, entry.install, entry.wasActive,
                   isAddonDistroInstalled(entry.addon.id));

      if (entry.install)
        entry.install.addListener(gUpdate);

      lastAddon = entry.addon;
    }

    showView(gSelect);
  }
};

var gSelect = {
  nodeID: "select",

  show: function gSelect_show() {
    this.updateButtons();
  },

  updateButtons: function gSelect_updateButtons() {
    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName == "separator")
        continue;

      if (row.action) {
        showButtons(false, false, true, false);
        return;
      }
    }

    showButtons(false, false, false, true);
  },

  next: function gSelect_next() {
    showView(gConfirm);
  },

  done: function gSelect_done() {
    window.close();
  }
};

var gConfirm = {
  nodeID: "confirm",

  show: function gConfirm_show() {
    showButtons(false, true, false, true);

    let box = document.getElementById("confirm-scrollbox").firstChild;
    while (box) {
      box.hidden = true;
      while (box.lastChild != box.firstChild)
        box.removeChild(box.lastChild);
      box = box.nextSibling;
    }

    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName == "separator")
        continue;

      let action = row.action;
      if (!action)
        continue;

      let list = document.getElementById(action + "-list");

      list.hidden = false;
      let item = document.createElement("hbox");
      item.setAttribute("id", row._addon.id);
      item.setAttribute("class", "addon");
      item.setAttribute("type", row._addon.type);
      item.setAttribute("name", row._addon.name);
      if (action == "update" || action == "enable")
        item.setAttribute("active", "true");
      list.appendChild(item);

      if (action == "update")
        showButtons(false, true, true, false);
    }
  },

  back: function gConfirm_back() {
    showView(gSelect);
  },

  next: function gConfirm_next() {
    showView(gUpdate);
  },

  done: function gConfirm_done() {
    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName != "separator")
        row.apply();
    }

    window.close();
  }
}

var gUpdate = {
  nodeID: "update",

  _progress: null,
  _waitingCount: 0,
  _completeCount: 0,
  _errorCount: 0,

  show: function gUpdate_show() {
    showButtons(true, false, false, false);

    this._progress = document.getElementById("update-progress");

    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName != "separator")
        row.apply();
    }

    this._progress.mode = "determined";
    this._progress.max = this._waitingCount;
    this._progress.value = this._completeCount;
  },

  checkComplete: function gUpdate_checkComplete() {
    this._progress.value = this._completeCount;
    if (this._completeCount < this._waitingCount)
      return;

    if (this._errorCount > 0) {
      showView(gErrors);
      return;
    }

    window.close();
  },

  onDownloadStarted: function gUpdate_onDownloadStarted(aInstall) {
    this._waitingCount++;
  },

  onDownloadFailed: function gUpdate_onDownloadFailed(aInstall) {
    this._errorCount++;
    this._completeCount++;
    this.checkComplete();
  },

  onInstallFailed: function gUpdate_onInstallFailed(aInstall) {
    this._errorCount++;
    this._completeCount++;
    this.checkComplete();
  },

  onInstallEnded: function gUpdate_onInstallEnded(aInstall) {
    this._completeCount++;
    this.checkComplete();
  }
};

var gErrors = {
  nodeID: "errors",

  show: function gErrors_show() {
    showButtons(false, false, false, true);
  },

  done: function gErrors_done() {
    window.close();
  }
};

window.addEventListener("load", function loadEventListener() {
                                         showView(gChecking); }, false);

// When closing the window cancel any pending or in-progress installs
window.addEventListener("unload", function unloadEventListener() {
  for (let id in gAddons) {
    let entry = gAddons[id];
    if (!entry.install)
      return;

    aEntry.install.removeListener(gUpdate);

    if (entry.install.state != AddonManager.STATE_INSTALLED &&
        entry.install.state != AddonManager.STATE_DOWNLOAD_FAILED &&
        entry.install.state != AddonManager.STATE_INSTALL_FAILED) {
      entry.install.cancel();
    }
  }
}, false);
