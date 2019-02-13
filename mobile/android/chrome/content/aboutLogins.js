/* Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

let debug = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "AboutLogins");

let gStringBundle = Services.strings.createBundle("chrome://browser/locale/aboutLogins.properties");

function copyStringAndToast(string, notifyString) {
  try {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(string);
    gChromeWin.NativeWindow.toast.show(notifyString, "short");
  } catch (e) {
    debug("Error copying from about:logins");
    gChromeWin.NativeWindow.toast.show(gStringBundle.GetStringFromName("loginsDetails.copyFailed"), "short");
  }
}

// Delay filtering while typing in MS
const FILTER_DELAY = 500;

let Logins = {
  _logins: [],
  _filterTimer: null,

  _getLogins: function() {
    let logins;
    try {
      logins = Services.logins.getAllLogins();
    } catch(e) {
      // Master password was not entered
      debug("Master password permissions error: " + e);
      logins = [];
    }

    logins.sort((a, b) => a.hostname.localeCompare(b.hostname));
    return this._logins = logins;
  },

  init: function () {
    window.addEventListener("popstate", this , false);

    Services.obs.addObserver(this, "passwordmgr-storage-changed", false);

    this._loadList(this._getLogins());

    let filterInput = document.getElementById("filter-input");
    let filterContainer = document.getElementById("filter-input-container");

    filterInput.addEventListener("input", (event) => {
      // Stop any in-progress filter timer
      if (this._filterTimer) {
        clearTimeout(this._filterTimer);
        this._filterTimer = null;
      }

      // Start a new timer
      this._filterTimer = setTimeout(() => {
        this._filter(event);
      }, FILTER_DELAY);
    }, false);

    filterInput.addEventListener("blur", (event) => {
      filterContainer.setAttribute("hidden", true);
    });

    document.getElementById("filter-button").addEventListener("click", (event) => {
      filterContainer.removeAttribute("hidden");
      filterInput.focus();
    }, false);

    document.getElementById("filter-clear").addEventListener("click", (event) => {
      // Stop any in-progress filter timer
      if (this._filterTimer) {
        clearTimeout(this._filterTimer);
        this._filterTimer = null;
      }

      filterInput.blur();
      filterInput.value = "";
      this._loadList(this._logins);
    }, false);

    this._showList();
  },

  uninit: function () {
    Services.obs.removeObserver(this, "passwordmgr-storage-changed");
    window.removeEventListener("popstate", this, false);
  },

  _loadList: function (logins) {
    let list = document.getElementById("logins-list");
    let newList = list.cloneNode(false);

    logins.forEach(login => {
      let item = this._createItemForLogin(login);
      newList.appendChild(item);
    });

    list.parentNode.replaceChild(newList, list);
  },

  _showList: function () {
    let list = document.getElementById("logins-list");
    list.removeAttribute("hidden");
  },

  _onLoginClick: function (event) {
    let loginItem = event.currentTarget;
    let login = loginItem.login;
    if (!login) {
      debug("No login!");
      return;
    }

    let prompt = new Prompt({
      window: window,
    });
    let menuItems = [
      { label: gStringBundle.GetStringFromName("loginsMenu.showPassword") },
      { label: gStringBundle.GetStringFromName("loginsMenu.copyPassword") },
      { label: gStringBundle.GetStringFromName("loginsMenu.copyUsername") },
      { label: gStringBundle.GetStringFromName("loginsMenu.delete") }
    ];

    prompt.setSingleChoiceItems(menuItems);
    prompt.show((data) => {
      // Switch on indices of buttons, as they were added when creating login item.
      switch (data.button) {
        case 0:
          let passwordPrompt = new Prompt({
            window: window,
            message: login.password,
            buttons: [
              gStringBundle.GetStringFromName("loginsDialog.copy"),
              gStringBundle.GetStringFromName("loginsDialog.cancel") ]
          }).show((data) => {
            switch (data.button) {
              case 0:
                // Corresponds to "Copy password" button.
                copyStringAndToast(login.password, gStringBundle.GetStringFromName("loginsDetails.passwordCopied"));
            }
          });
          break;
        case 1:
          copyStringAndToast(login.password, gStringBundle.GetStringFromName("loginsDetails.passwordCopied"));
          break;
        case 2:
          copyStringAndToast(login.username, gStringBundle.GetStringFromName("loginsDetails.usernameCopied"));
          break;
        case 3:
          let confirmPrompt = new Prompt({
            window: window,
            message: gStringBundle.GetStringFromName("loginsDialog.confirmDelete"),
            buttons: [
              gStringBundle.GetStringFromName("loginsDialog.confirm"),
              gStringBundle.GetStringFromName("loginsDialog.cancel") ]
          });
          confirmPrompt.show((data) => {
            switch (data.button) {
              case 0:
                // Corresponds to "confirm" button.
                Services.logins.removeLogin(login);
            }
          });
      }
    });
  },

  _createItemForLogin: function (login) {
    let loginItem = document.createElement("div");

    loginItem.setAttribute("loginID", login.guid);
    loginItem.className = "login-item list-item";

    loginItem.addEventListener("click", this, true);

    // Create item icon.
    let img = document.createElement("div");
    img.className = "icon";

    // Load favicon from cache.
    Messaging.sendRequestForResult({
      type: "Favicon:CacheLoad",
      url: login.hostname,
    }).then(function(faviconUrl) {
      img.style.backgroundImage= "url('" + faviconUrl + "')";
      img.style.visibility = "visible";
    }, function(data) {
      debug("Favicon cache failure : " + data);
      img.style.visibility = "visible";
    });
    loginItem.appendChild(img);

    // Create item details.
    let inner = document.createElement("div");
    inner.className = "inner";

    let details = document.createElement("div");
    details.className = "details";
    inner.appendChild(details);

    let titlePart = document.createElement("div");
    titlePart.className = "hostname";
    titlePart.textContent = login.hostname;
    details.appendChild(titlePart);

    let versionPart = document.createElement("div");
    versionPart.textContent = login.httpRealm;
    versionPart.className = "realm";
    details.appendChild(versionPart);

    let descPart = document.createElement("div");
    descPart.textContent = login.username;
    descPart.className = "username";
    inner.appendChild(descPart);

    loginItem.appendChild(inner);
    loginItem.login = login;
    return loginItem;
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "popstate": {
        this._onPopState(event);
        break;
      }
      case "click": {
        this._onLoginClick(event);
        break;
      }
    }
  },

  observe: function (subject, topic, data) {
    switch(topic) {
      case "passwordmgr-storage-changed": {
        // Reload logins content.
        this._loadList(this._getLogins());
        break;
      }
    }
  },

  _filter: function(event) {
    let value = event.target.value.toLowerCase();
    let logins = this._logins.filter((login) => {
      if (login.hostname.toLowerCase().indexOf(value) != -1) {
        return true;
      }
      if (login.username &&
          login.username.toLowerCase().indexOf(value) != -1) {
        return true;
      }
      if (login.httpRealm &&
          login.httpRealm.toLowerCase().indexOf(value) != -1) {
        return true;
      }
      return false;
    });

    this._loadList(logins);
  }
};

window.addEventListener("load", Logins.init.bind(Logins), false);
window.addEventListener("unload", Logins.uninit.bind(Logins), false);
