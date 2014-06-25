// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://services-sync/main.js");
Cu.import("resource://gre/modules/PlacesUtils.jsm", this);

/**
 * Wraps a list/grid control implementing nsIDOMXULSelectControlElement and
 * fills it with the user's synced tabs.
 *
 * Note, the Sync module takes care of initializing the sync service. We should
 * not make calls that start sync or sync tabs since this module loads really
 * early during startup.
 *
 * @param    aSet         Control implementing nsIDOMXULSelectControlElement.
 * @param    aSetUIAccess The UI element that should be hidden when Sync is
 *                          disabled. Must sanely support 'hidden' attribute.
 *                          You may only have one UI access point at this time.
 */
function RemoteTabsView(aSet, aSetUIAccessList) {
  View.call(this, aSet);

  this._uiAccessElements = aSetUIAccessList;

  // Sync uses special voodoo observers.
  // If you want to change this code, talk to the fx-si team
  Weave.Svc.Obs.add("weave:service:sync:finish", this);
  Weave.Svc.Obs.add("weave:service:start-over", this);

  if (this.isSyncEnabled() ) {
    this.populateGrid();
  }
  else {
    this.setUIAccessVisible(false);
  }
}

RemoteTabsView.prototype = Util.extend(Object.create(View.prototype), {
  _set: null,
  _uiAccessElements: [],

  handleItemClick: function tabview_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    StartUI.goToURI(url);
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "weave:service:sync:finish":
        this.populateGrid();
        break;
      case "weave:service:start-over":
        this.setUIAccessVisible(false);
        break;
    }
  },

  setUIAccessVisible: function setUIAccessVisible(aVisible) {
    for (let elem of this._uiAccessElements) {
      elem.hidden = !aVisible;
    }
  },

  getIcon: function (iconUri, defaultIcon) {
    try {
      let iconURI = Weave.Utils.makeURI(iconUri);
      return PlacesUtils.favicons.getFaviconLinkForIcon(iconURI).spec;
    } catch(ex) {
      // Do nothing.
    }

    // Just give the provided default icon or the system's default.
    return defaultIcon || PlacesUtils.favicons.defaultFavicon.spec;
  },

  populateGrid: function populateGrid() {

    let tabsEngine = Weave.Service.engineManager.get("tabs");
    let list = this._set;
    let seenURLs = new Set();
    let localURLs = tabsEngine.getOpenURLs();

    // Clear grid, We don't know what has happened to tabs since last sync
    // Also can result in duplicate tabs(bug 864614)
    this._set.clearAll();
    let show = false;
    for (let [guid, client] in Iterator(tabsEngine.getAllClients())) {
      client.tabs.forEach(function({title, urlHistory, icon}) {
        let url = urlHistory[0];
        if (!url || getOpenURLs.has(url) || seenURLs.has(url)) {
          return;
        }
        seenURLs.add(url);
        show = true;

        // If we wish to group tabs by client, we should be looking for records
        //  of {type:client, clientName, class:{mobile, desktop}} and will
        //  need to readd logic to reset seenURLs for each client.

        let item = this._set.appendItem((title || url), url);
        item.setAttribute("iconURI", this.getIcon(icon));

      }, this);
    }
    this.setUIAccessVisible(show);
    this._set.arrangeItems();
  },

  destruct: function destruct() {
    Weave.Svc.Obs.remove("weave:engine:sync:finish", this);
    Weave.Svc.Obs.remove("weave:service:logout:start-over", this);
    View.prototype.destruct.call(this);
  },

  isSyncEnabled: function isSyncEnabled() {
    return (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED);
  }

});

let RemoteTabsStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-remotetabs-grid"); },

  init: function init() {
    let vbox = document.getElementById("start-remotetabs");
    let uiList = [vbox];
    this._view = new RemoteTabsView(this._grid, uiList);
    this._grid.removeAttribute("fade");
  },

  uninit: function uninit() {
    if (this._view) {
      this._view.destruct();
    }
  },
};
