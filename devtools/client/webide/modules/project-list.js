/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Services = require("Services");
const {AppProjects} = require("devtools/client/webide/modules/app-projects");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const EventEmitter = require("devtools/shared/event-emitter");
const utils = require("devtools/client/webide/modules/utils");
const Telemetry = require("devtools/client/shared/telemetry");

const Strings =
  Services.strings.createBundle("chrome://devtools/locale/webide.properties");

const TELEMETRY_WEBIDE_NEW_PROJECT_COUNT = "DEVTOOLS_WEBIDE_NEW_PROJECT_COUNT";

var ProjectList;

module.exports = ProjectList = function(win, parentWindow) {
  EventEmitter.decorate(this);
  this._doc = win.document;
  this._UI = parentWindow.UI;
  this._parentWindow = parentWindow;
  this._telemetry = new Telemetry();
  this._panelNodeEl = "div";

  this.onWebIDEUpdate = this.onWebIDEUpdate.bind(this);
  this._UI.on("webide-update", this.onWebIDEUpdate);

  AppManager.init();
  this.appManagerUpdate = this.appManagerUpdate.bind(this);
  AppManager.on("app-manager-update", this.appManagerUpdate);
};

ProjectList.prototype = {
  get doc() {
    return this._doc;
  },

  appManagerUpdate: function(what, details) {
    // Got a message from app-manager.js
    // See AppManager.update() for descriptions of what these events mean.
    switch (what) {
      case "project-removed":
      case "runtime-targets":
      case "connection":
        this.update(details);
        break;
      case "project":
        this.updateCommands();
        this.update(details);
        break;
    }
  },

  onWebIDEUpdate: function(what, details) {
    if (what == "busy" || what == "unbusy") {
      this.updateCommands();
    }
  },

  /**
   * testOptions: {       chrome mochitest support
   *   folder: nsIFile,   where to store the app
   *   index: Number,     index of the app in the template list
   *   name: String       name of the app
   * }
   */
  newApp: function(testOptions) {
    const parentWindow = this._parentWindow;
    const self = this;
    return this._UI.busyUntil((async function() {
      // Open newapp.xul, which will feed ret.location
      const ret = {location: null, testOptions: testOptions};
      parentWindow.openDialog("chrome://webide/content/newapp.xul", "newapp", "chrome,modal", ret);
      if (!ret.location) {
        return;
      }

      // Retrieve added project
      const project = AppProjects.get(ret.location);

      // Select project
      AppManager.selectedProject = project;

      self._telemetry.getHistogramById(TELEMETRY_WEBIDE_NEW_PROJECT_COUNT).add(true);
    })(), "creating new app");
  },

  importPackagedApp: function(location) {
    const parentWindow = this._parentWindow;
    const UI = this._UI;
    return UI.busyUntil((async function() {
      const directory = await utils.getPackagedDirectory(parentWindow, location);

      if (!directory) {
        // User cancelled directory selection
        return;
      }

      await UI.importAndSelectApp(directory);
    })(), "importing packaged app");
  },

  importHostedApp: function(location) {
    const parentWindow = this._parentWindow;
    const UI = this._UI;
    return UI.busyUntil((async function() {
      const url = utils.getHostedURL(parentWindow, location);

      if (!url) {
        return;
      }

      await UI.importAndSelectApp(url);
    })(), "importing hosted app");
  },

  /**
   * opts: {
   *   panel: Object,     currenl project panel node
   *   name: String,      name of the project
   *   icon: String       path of the project icon
   * }
   */
  _renderProjectItem: function(opts) {
    const span = opts.panel.querySelector("span") || this._doc.createElement("span");
    span.textContent = opts.name;
    const icon = opts.panel.querySelector("img") || this._doc.createElement("img");
    icon.className = "project-image";
    icon.setAttribute("src", opts.icon);
    opts.panel.appendChild(icon);
    opts.panel.appendChild(span);
    opts.panel.setAttribute("title", opts.name);
  },

  refreshTabs: function() {
    if (AppManager.connected) {
      return AppManager.listTabs().then(() => {
        this.updateTabs();
      }).catch(console.error);
    }
  },

  updateTabs: function() {
    const tabsHeaderNode = this._doc.querySelector("#panel-header-tabs");
    const tabsNode = this._doc.querySelector("#project-panel-tabs");

    while (tabsNode.hasChildNodes()) {
      tabsNode.firstChild.remove();
    }

    if (!AppManager.connected) {
      tabsHeaderNode.setAttribute("hidden", "true");
      return;
    }

    const tabs = AppManager.tabStore.tabs;

    tabsHeaderNode.removeAttribute("hidden");

    for (let i = 0; i < tabs.length; i++) {
      const tab = tabs[i];
      const URL = this._parentWindow.URL;
      let url;
      try {
        url = new URL(tab.url);
      } catch (e) {
        // Don't try to handle invalid URLs
        continue;
      }
      // Wanted to use nsIFaviconService here, but it only works for visited
      // tabs, so that's no help for any remote tabs.  Maybe some favicon wizard
      // knows how to get high-res favicons easily, or we could offer actor
      // support for this (bug 1061654).
      if (url.origin) {
        tab.favicon = url.origin + "/favicon.ico";
      }
      tab.name = tab.title || Strings.GetStringFromName("project_tab_loading");
      if (url.protocol.startsWith("http")) {
        tab.name = url.hostname + ": " + tab.name;
      }
      const panelItemNode = this._doc.createElement(this._panelNodeEl);
      panelItemNode.className = "panel-item";
      tabsNode.appendChild(panelItemNode);
      this._renderProjectItem({
        panel: panelItemNode,
        name: tab.name,
        icon: tab.favicon || AppManager.DEFAULT_PROJECT_ICON,
      });
      panelItemNode.addEventListener("click", () => {
        AppManager.selectedProject = {
          type: "tab",
          app: tab,
          icon: tab.favicon || AppManager.DEFAULT_PROJECT_ICON,
          location: tab.url,
          name: tab.name,
        };
      }, true);
    }

    return Promise.resolve();
  },

  updateApps: function() {
    const doc = this._doc;
    const runtimeappsHeaderNode = doc.querySelector("#panel-header-runtimeapps");
    let sortedApps = [];
    for (const [/* manifestURL */, app] of AppManager.apps) {
      sortedApps.push(app);
    }
    sortedApps = sortedApps.sort((a, b) => {
      return a.manifest.name > b.manifest.name;
    });
    const mainProcess = AppManager.isMainProcessDebuggable();
    if (AppManager.connected && (sortedApps.length > 0 || mainProcess)) {
      runtimeappsHeaderNode.removeAttribute("hidden");
    } else {
      runtimeappsHeaderNode.setAttribute("hidden", "true");
    }

    const runtimeAppsNode = doc.querySelector("#project-panel-runtimeapps");
    while (runtimeAppsNode.hasChildNodes()) {
      runtimeAppsNode.firstChild.remove();
    }

    if (mainProcess) {
      const panelItemNode = doc.createElement(this._panelNodeEl);
      panelItemNode.className = "panel-item";
      this._renderProjectItem({
        panel: panelItemNode,
        name: Strings.GetStringFromName("mainProcess_label"),
        icon: AppManager.DEFAULT_PROJECT_ICON,
      });
      runtimeAppsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        AppManager.selectedProject = {
          type: "mainProcess",
          name: Strings.GetStringFromName("mainProcess_label"),
          icon: AppManager.DEFAULT_PROJECT_ICON,
        };
      }, true);
    }

    for (let i = 0; i < sortedApps.length; i++) {
      const app = sortedApps[i];
      const panelItemNode = doc.createElement(this._panelNodeEl);
      panelItemNode.className = "panel-item";
      this._renderProjectItem({
        panel: panelItemNode,
        name: app.manifest.name,
        icon: app.iconURL || AppManager.DEFAULT_PROJECT_ICON,
      });
      runtimeAppsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        AppManager.selectedProject = {
          type: "runtimeApp",
          app: app.manifest,
          icon: app.iconURL || AppManager.DEFAULT_PROJECT_ICON,
          name: app.manifest.name,
        };
      }, true);
    }

    return Promise.resolve();
  },

  updateCommands: function() {
    const doc = this._doc;

    const newAppCmd = doc.querySelector("#new-app");
    const packagedAppCmd = doc.querySelector("#packaged-app");
    const hostedAppCmd = doc.querySelector("#hosted-app");

    if (!newAppCmd || !packagedAppCmd || !hostedAppCmd) {
      return;
    }

    if (this._parentWindow.document.querySelector("window").classList.contains("busy")) {
      newAppCmd.setAttribute("disabled", "true");
      packagedAppCmd.setAttribute("disabled", "true");
      hostedAppCmd.setAttribute("disabled", "true");
      return;
    }

    newAppCmd.removeAttribute("disabled");
    packagedAppCmd.removeAttribute("disabled");
    hostedAppCmd.removeAttribute("disabled");
  },

  /**
   * Trigger an update of the project and remote runtime list.
   * @param options object (optional)
   *        An |options| object containing a type of |apps| or |tabs| will limit
   *        what is updated to only those sections.
   */
  update: function(options) {
    if (options && options.type === "apps") {
      return this.updateApps();
    } else if (options && options.type === "tabs") {
      return this.updateTabs();
    }

    return new Promise((resolve, reject) => {
      const doc = this._doc;
      const projectsNode = doc.querySelector("#project-panel-projects");

      while (projectsNode.hasChildNodes()) {
        projectsNode.firstChild.remove();
      }

      AppProjects.load().then(() => {
        const projects = AppProjects.projects;
        for (let i = 0; i < projects.length; i++) {
          const project = projects[i];
          const panelItemNode = doc.createElement(this._panelNodeEl);
          panelItemNode.className = "panel-item";
          projectsNode.appendChild(panelItemNode);
          if (!project.validationStatus) {
            // The result of the validation process (storing names, icons, …) is not stored in
            // the IndexedDB database when App Manager v1 is used.
            // We need to run the validation again and update the name and icon of the app.
            AppManager.validateAndUpdateProject(project).then(() => {
              this._renderProjectItem({
                panel: panelItemNode,
                name: project.name,
                icon: project.icon,
              });
            });
          } else {
            this._renderProjectItem({
              panel: panelItemNode,
              name: project.name || AppManager.DEFAULT_PROJECT_NAME,
              icon: project.icon || AppManager.DEFAULT_PROJECT_ICON,
            });
          }
          panelItemNode.addEventListener("click", () => {
            AppManager.selectedProject = project;
          }, true);
        }

        resolve();
      }, reject);

      // List remote apps and the main process, if they exist
      this.updateApps();

      // Build the tab list right now, so it's fast...
      this.updateTabs();

      // But re-list them and rebuild, in case any tabs navigated since the last
      // time they were listed.
      if (AppManager.connected) {
        AppManager.listTabs().then(() => {
          this.updateTabs();
        }).catch(console.error);
      }
    });
  },

  destroy: function() {
    this._doc = null;
    AppManager.off("app-manager-update", this.appManagerUpdate);
    this._UI.off("webide-update", this.onWebIDEUpdate);
    this._UI = null;
    this._parentWindow = null;
    this._panelNodeEl = null;
  },
};
