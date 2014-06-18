/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");

var {Promise: promise} = require("resource://gre/modules/Promise.jsm");
var EventEmitter = require("devtools/toolkit/event-emitter");
var Telemetry = require("devtools/shared/telemetry");

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * ToolSidebar provides methods to register tabs in the sidebar.
 * It's assumed that the sidebar contains a xul:tabbox.
 *
 * @param {Node} tabbox
 *  <tabbox> node;
 * @param {ToolPanel} panel
 *  Related ToolPanel instance;
 * @param {String} uid
 *  Unique ID
 * @param {Boolean} showTabstripe
 *  Show the tabs.
 */
function ToolSidebar(tabbox, panel, uid, showTabstripe=true)
{
  EventEmitter.decorate(this);

  this._tabbox = tabbox;
  this._uid = uid;
  this._panelDoc = this._tabbox.ownerDocument;
  this._toolPanel = panel;

  try {
    this._width = Services.prefs.getIntPref("devtools.toolsidebar-width." + this._uid);
  } catch(e) {}

  this._telemetry = new Telemetry();

  this._tabbox.tabpanels.addEventListener("select", this, true);

  this._tabs = new Map();

  if (!showTabstripe) {
    this._tabbox.setAttribute("hidetabs", "true");
  }
}

exports.ToolSidebar = ToolSidebar;

ToolSidebar.prototype = {
  /**
   * Register a tab. A tab is a document.
   * The document must have a title, which will be used as the name of the tab.
   *
   * @param {string} tab uniq id
   * @param {string} url
   */
  addTab: function ToolSidebar_addTab(id, url, selected=false) {
    let iframe = this._panelDoc.createElementNS(XULNS, "iframe");
    iframe.className = "iframe-" + id;
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("src", url);
    iframe.tooltip = "aHTMLTooltip";

    let tab = this._tabbox.tabs.appendItem();
    tab.setAttribute("label", ""); // Avoid showing "undefined" while the tab is loading

    let onIFrameLoaded = () => {
      tab.setAttribute("label", iframe.contentDocument.title);
      iframe.removeEventListener("load", onIFrameLoaded, true);
      if ("setPanel" in iframe.contentWindow) {
        iframe.contentWindow.setPanel(this._toolPanel, iframe);
      }
      this.emit(id + "-ready");
    };

    iframe.addEventListener("load", onIFrameLoaded, true);

    let tabpanel = this._panelDoc.createElementNS(XULNS, "tabpanel");
    tabpanel.setAttribute("id", "sidebar-panel-" + id);
    tabpanel.appendChild(iframe);
    this._tabbox.tabpanels.appendChild(tabpanel);

    this._tooltip = this._panelDoc.createElementNS(XULNS, "tooltip");
    this._tooltip.id = "aHTMLTooltip";
    tabpanel.appendChild(this._tooltip);
    this._tooltip.page = true;

    tab.linkedPanel = "sidebar-panel-" + id;

    // We store the index of this tab.
    this._tabs.set(id, tab);

    if (selected) {
      // For some reason I don't understand, if we call this.select in this
      // event loop (after inserting the tab), the tab will never get the
      // the "selected" attribute set to true.
      this._panelDoc.defaultView.setTimeout(() => {
        this.select(id);
      }, 10);
    }

    this.emit("new-tab-registered", id);
  },

  /**
   * Select a specific tab.
   */
  select: function ToolSidebar_select(id) {
    let tab = this._tabs.get(id);
    if (tab) {
      this._tabbox.selectedTab = tab;
    }
  },

  /**
   * Return the id of the selected tab.
   */
  getCurrentTabID: function ToolSidebar_getCurrentTabID() {
    let currentID = null;
    for (let [id, tab] of this._tabs) {
      if (this._tabbox.tabs.selectedItem == tab) {
        currentID = id;
        break;
      }
    }
    return currentID;
  },

  /**
   * Returns the requested tab based on the id.
   *
   * @param String id
   *        unique id of the requested tab.
   */
  getTab: function ToolSidebar_getTab(id) {
    return this._tabbox.tabpanels.querySelector("#sidebar-panel-" + id);
  },

  /**
   * Event handler.
   */
  handleEvent: function ToolSidebar_eventHandler(event) {
    if (event.type == "select") {
      if (this._currentTool == this.getCurrentTabID()) {
        // Tool hasn't changed.
        return;
      }

      let previousTool = this._currentTool;
      this._currentTool = this.getCurrentTabID();
      if (previousTool) {
        this._telemetry.toolClosed(previousTool);
        this.emit(previousTool + "-unselected");
      }

      this._telemetry.toolOpened(this._currentTool);
      this.emit(this._currentTool + "-selected");
      this.emit("select", this._currentTool);
    }
  },

  /**
   * Toggle sidebar's visibility state.
   */
  toggle: function ToolSidebar_toggle() {
    if (this._tabbox.hasAttribute("hidden")) {
      this.show();
    } else {
      this.hide();
    }
  },

  /**
   * Show the sidebar.
   */
  show: function ToolSidebar_show() {
    if (this._width) {
      this._tabbox.width = this._width;
    }
    this._tabbox.removeAttribute("hidden");
  },

  /**
   * Show the sidebar.
   */
  hide: function ToolSidebar_hide() {
    Services.prefs.setIntPref("devtools.toolsidebar-width." + this._uid, this._tabbox.width);
    this._tabbox.setAttribute("hidden", "true");
  },

  /**
   * Return the window containing the tab content.
   */
  getWindowForTab: function ToolSidebar_getWindowForTab(id) {
    if (!this._tabs.has(id)) {
      return null;
    }

    let panel = this._panelDoc.getElementById(this._tabs.get(id).linkedPanel);
    return panel.firstChild.contentWindow;
  },

  /**
   * Clean-up.
   */
  destroy: function ToolSidebar_destroy() {
    if (this._destroyed) {
      return promise.resolve(null);
    }
    this._destroyed = true;

    Services.prefs.setIntPref("devtools.toolsidebar-width." + this._uid, this._tabbox.width);

    this._tabbox.tabpanels.removeEventListener("select", this, true);

    while (this._tabbox.tabpanels.hasChildNodes()) {
      this._tabbox.tabpanels.removeChild(this._tabbox.tabpanels.firstChild);
    }

    while (this._tabbox.tabs.hasChildNodes()) {
      this._tabbox.tabs.removeChild(this._tabbox.tabs.firstChild);
    }

    if (this._currentTool) {
      this._telemetry.toolClosed(this._currentTool);
    }

    this._tabs = null;
    this._tabbox = null;
    this._panelDoc = null;
    this._toolPanel = null;

    return promise.resolve(null);
  },
}
