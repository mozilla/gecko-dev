/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

/**
 * This object represents replacement for ToolSidebar
 * implemented in devtools/client/framework/sidebar.js module
 *
 * This new component is part of devtools.html aimed at
 * removing XUL and use HTML for entire DevTools UI.
 * There are currently two implementation of the side bar since
 * the `sidebar.js` module (mentioned above) is still used by
 * other panels.
 * As soon as all panels are using this HTML based
 * implementation it can be removed.
 */
function ToolSidebar(tabbox, panel, uid, options = {}) {
  EventEmitter.decorate(this);

  this._tabbox = tabbox;
  this._uid = uid;
  this._panelDoc = this._tabbox.ownerDocument;
  this._toolPanel = panel;
  this._options = options;

  if (!options.disableTelemetry) {
    this._telemetry = this._toolPanel.telemetry;
  }

  this._tabs = [];

  if (this._options.hideTabstripe) {
    this._tabbox.setAttribute("hidetabs", "true");
  }

  this.render();

  this._toolPanel.emit("sidebar-created", this);
}

exports.ToolSidebar = ToolSidebar;

ToolSidebar.prototype = {
  TABPANEL_ID_PREFIX: "sidebar-panel-",

  // React

  get React() {
    return this._toolPanel.React;
  },

  get ReactDOM() {
    return this._toolPanel.ReactDOM;
  },

  get browserRequire() {
    return this._toolPanel.browserRequire;
  },

  get InspectorTabPanel() {
    return this._toolPanel.InspectorTabPanel;
  },

  get TabBar() {
    return this._toolPanel.TabBar;
  },

  // Rendering

  render: function() {
    const sidebar = this.TabBar({
      menuDocument: this._toolPanel._toolbox.doc,
      showAllTabsMenu: true,
      sidebarToggleButton: this._options.sidebarToggleButton,
      onSelect: this.handleSelectionChange.bind(this),
    });

    this._tabbar = this.ReactDOM.render(sidebar, this._tabbox);
  },

  /**
   * Adds all the queued tabs.
   */
  addAllQueuedTabs: function() {
    this._tabbar.addAllQueuedTabs();
  },

  /**
   * Register a side-panel tab.
   *
   * @param {String} tab uniq id
   * @param {String} title tab title
   * @param {React.Component} panel component. See `InspectorPanelTab` as an example.
   * @param {Boolean} selected true if the panel should be selected
   * @param {Number} index the position where the tab should be inserted
   */
  addTab: function(id, title, panel, selected, index) {
    this._tabbar.addTab(id, title, selected, panel, null, index);
    this.emit("new-tab-registered", id);
  },

  /**
   * Helper API for adding side-panels that use existing DOM nodes
   * (defined within inspector.xhtml) as the content.
   *
   * @param {String} tab uniq id
   * @param {String} title tab title
   * @param {Boolean} selected true if the panel should be selected
   * @param {Number} index the position where the tab should be inserted
   */
  addExistingTab: function(id, title, selected, index) {
    const panel = this.InspectorTabPanel({
      id: id,
      idPrefix: this.TABPANEL_ID_PREFIX,
      key: id,
      title: title,
    });

    this.addTab(id, title, panel, selected, index);
  },

  /**
   * Helper API for adding side-panels that use existing <iframe> nodes
   * (defined within inspector.xhtml) as the content.
   * The document must have a title, which will be used as the name of the tab.
   *
   * @param {String} tab uniq id
   * @param {String} title tab title
   * @param {String} url
   * @param {Boolean} selected true if the panel should be selected
   * @param {Number} index the position where the tab should be inserted
   */
  addFrameTab: function(id, title, url, selected, index) {
    const panel = this.InspectorTabPanel({
      id: id,
      idPrefix: this.TABPANEL_ID_PREFIX,
      key: id,
      title: title,
      url: url,
      onMount: this.onSidePanelMounted.bind(this),
      onUnmount: this.onSidePanelUnmounted.bind(this),
    });

    this.addTab(id, title, panel, selected, index);
  },

  /**
   * Queues a side-panel tab to be added..
   *
   * @param {String} tab uniq id
   * @param {String} title tab title
   * @param {React.Component} panel component. See `InspectorPanelTab` as an example.
   * @param {Boolean} selected true if the panel should be selected
   * @param {Number} index the position where the tab should be inserted
   */
  queueTab: function(id, title, panel, selected, index) {
    this._tabbar.queueTab(id, title, selected, panel, null, index);
    this.emit("new-tab-registered", id);
  },

  /**
   * Helper API for queuing side-panels that use existing DOM nodes
   * (defined within inspector.xhtml) as the content.
   *
   * @param {String} tab uniq id
   * @param {String} title tab title
   * @param {Boolean} selected true if the panel should be selected
   * @param {Number} index the position where the tab should be inserted
   */
  queueExistingTab: function(id, title, selected, index) {
    const panel = this.InspectorTabPanel({
      id: id,
      idPrefix: this.TABPANEL_ID_PREFIX,
      key: id,
      title: title,
    });

    this.queueTab(id, title, panel, selected, index);
  },

  /**
   * Helper API for queuing side-panels that use existing <iframe> nodes
   * (defined within inspector.xhtml) as the content.
   * The document must have a title, which will be used as the name of the tab.
   *
   * @param {String} tab uniq id
   * @param {String} title tab title
   * @param {String} url
   * @param {Boolean} selected true if the panel should be selected
   * @param {Number} index the position where the tab should be inserted
   */
  queueFrameTab: function(id, title, url, selected, index) {
    const panel = this.InspectorTabPanel({
      id: id,
      idPrefix: this.TABPANEL_ID_PREFIX,
      key: id,
      title: title,
      url: url,
      onMount: this.onSidePanelMounted.bind(this),
      onUnmount: this.onSidePanelUnmounted.bind(this),
    });

    this.queueTab(id, title, panel, selected, index);
  },

  onSidePanelMounted: function(content, props) {
    const iframe = content.querySelector("iframe");
    if (!iframe || iframe.getAttribute("src")) {
      return;
    }

    const onIFrameLoaded = (event) => {
      iframe.removeEventListener("load", onIFrameLoaded, true);

      const doc = event.target;
      const win = doc.defaultView;
      if ("setPanel" in win) {
        win.setPanel(this._toolPanel, iframe);
      }
      this.emit(props.id + "-ready");
    };

    iframe.addEventListener("load", onIFrameLoaded, true);
    iframe.setAttribute("src", props.url);
  },

  onSidePanelUnmounted: function(content, props) {
    const iframe = content.querySelector("iframe");
    if (!iframe || !iframe.hasAttribute("src")) {
      return;
    }

    const win = iframe.contentWindow;
    if ("destroy" in win) {
      win.destroy(this._toolPanel, iframe);
    }

    iframe.removeAttribute("src");
  },

  /**
   * Remove an existing tab.
   * @param {String} tabId The ID of the tab that was used to register it, or
   * the tab id attribute value if the tab existed before the sidebar
   * got created.
   * @param {String} tabPanelId Optional. If provided, this ID will be used
   * instead of the tabId to retrieve and remove the corresponding <tabpanel>
   */
  async removeTab(tabId, tabPanelId) {
    this._tabbar.removeTab(tabId);

    const win = this.getWindowForTab(tabId);
    if (win && ("destroy" in win)) {
      await win.destroy();
    }

    this.emit("tab-unregistered", tabId);
  },

  /**
   * Show or hide a specific tab.
   * @param {Boolean} isVisible True to show the tab/tabpanel, False to hide it.
   * @param {String} id The ID of the tab to be hidden.
   */
  toggleTab: function(isVisible, id) {
    this._tabbar.toggleTab(id, isVisible);
  },

  /**
   * Select a specific tab.
   */
  select: function(id) {
    this._tabbar.select(id);
  },

  /**
   * Return the id of the selected tab.
   */
  getCurrentTabID: function() {
    return this._currentTool;
  },

  /**
   * Returns the requested tab panel based on the id.
   * @param {String} id
   * @return {DOMNode}
   */
  getTabPanel: function(id) {
    // Search with and without the ID prefix as there might have been existing
    // tabpanels by the time the sidebar got created
    return this._panelDoc.querySelector("#" +
      this.TABPANEL_ID_PREFIX + id + ", #" + id);
  },

  /**
   * Event handler.
   */
  handleSelectionChange: function(id) {
    if (this._destroyed) {
      return;
    }

    const previousTool = this._currentTool;
    if (previousTool) {
      this.emit(previousTool + "-unselected");
    }

    this._currentTool = id;

    this.updateTelemetryOnChange(id, previousTool);
    this.emit(this._currentTool + "-selected");
    this.emit("select", this._currentTool);
  },

  /**
   * Log toolClosed and toolOpened events on telemetry.
   *
   * @param  {String} currentToolId
   *         id of the tool being selected.
   * @param  {String} previousToolId
   *         id of the previously selected tool.
   */
  updateTelemetryOnChange: function(currentToolId, previousToolId) {
    if (currentToolId === previousToolId || !this._telemetry) {
      // Skip telemetry if the tool id did not change or telemetry is unavailable.
      return;
    }

    const sessionId = this._toolPanel._toolbox.sessionId;

    currentToolId = this.getTelemetryPanelNameOrOther(currentToolId);

    if (previousToolId) {
      previousToolId = this.getTelemetryPanelNameOrOther(previousToolId);
      this._telemetry.toolClosed(previousToolId, sessionId, this);

      this._telemetry.recordEvent("sidepanel_changed", "inspector", null,
        {
          "oldpanel": previousToolId,
          "newpanel": currentToolId,
          "os": this._telemetry.osNameAndVersion,
          "session_id": sessionId,
        }
      );
    }
    this._telemetry.toolOpened(currentToolId, sessionId, this);
  },

  /**
   * Returns a panel id in the case of built in panels or "other" in the case of
   * third party panels. This is necessary due to limitations in addon id strings,
   * the permitted length of event telemetry property values and what we actually
   * want to see in our telemetry.
   *
   * @param {String} id
   *        The panel id we would like to process.
   */
  getTelemetryPanelNameOrOther: function(id) {
    if (!this._toolNames) {
      // Get all built in tool ids. We identify third party tool ids by checking
      // for a "-", which shows it originates from an addon.
      const ids = this._tabbar.state.tabs.map(({ id: toolId }) => {
        return toolId.includes("-") ? "other" : toolId;
      });

      this._toolNames = new Set(ids);
    }

    if (!this._toolNames.has(id)) {
      return "other";
    }

    return id;
  },

  /**
   * Show the sidebar.
   *
   * @param  {String} id
   *         The sidebar tab id to select.
   */
  show: function(id) {
    this._tabbox.removeAttribute("hidden");

    // If an id is given, select the corresponding sidebar tab.
    if (id) {
      this.select(id);
    }

    this.emit("show");
  },

  /**
   * Show the sidebar.
   */
  hide: function() {
    this._tabbox.setAttribute("hidden", "true");

    this.emit("hide");
  },

  /**
   * Return the window containing the tab content.
   */
  getWindowForTab: function(id) {
    // Get the tabpanel and make sure it contains an iframe
    const panel = this.getTabPanel(id);
    if (!panel || !panel.firstElementChild || !panel.firstElementChild.contentWindow) {
      return null;
    }

    return panel.firstElementChild.contentWindow;
  },

  /**
   * Clean-up.
   */
  async destroy() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this.emit("destroy");

    // Note that we check for the existence of this._tabbox.tabpanels at each
    // step as the container window may have been closed by the time one of the
    // panel's destroy promise resolves.
    const tabpanels = [...this._tabbox.querySelectorAll(".tab-panel-box")];
    for (const panel of tabpanels) {
      const iframe = panel.querySelector("iframe");
      if (!iframe) {
        continue;
      }
      const win = iframe.contentWindow;
      if (win && ("destroy" in win)) {
        await win.destroy();
      }
      panel.remove();
    }

    if (this._currentTool && this._telemetry) {
      const sessionId = this._toolPanel._toolbox.sessionId;
      this._telemetry.toolClosed(this._currentTool, sessionId, this);
    }

    this._toolPanel.emit("sidebar-destroyed", this);

    this._tabs = null;
    this._tabbox = null;
    this._telemetry = null;
    this._panelDoc = null;
    this._toolPanel = null;
  },
};
