/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const { HarCollector } = require("./har-collector");
const { HarExporter } = require("./har-exporter");
const { HarUtils } = require("./har-utils");

const prefDomain = "devtools.netmonitor.har.";

// Helper tracer. Should be generic sharable by other modules (bug 1171927)
const trace = {
  log: function(...args) {
  },
};

/**
 * This object is responsible for automated HAR export. It listens
 * for Network activity, collects all HTTP data and triggers HAR
 * export when the page is loaded.
 *
 * The user needs to enable the following preference to make the
 * auto-export work: devtools.netmonitor.har.enableAutoExportToFile
 *
 * HAR files are stored within directory that is specified in this
 * preference: devtools.netmonitor.har.defaultLogDir
 *
 * If the default log directory preference isn't set the following
 * directory is used by default: <profile>/har/logs
 */
function HarAutomation(toolbox) {
  this.initialize(toolbox);
}

HarAutomation.prototype = {
  // Initialization

  initialize: function(toolbox) {
    this.toolbox = toolbox;

    const target = toolbox.target;
    this.startMonitoring(target.client, target.form);
  },

  destroy: function() {
    if (this.collector) {
      this.collector.stop();
    }

    if (this.tabWatcher) {
      this.tabWatcher.disconnect();
    }
  },

  // Automation

  startMonitoring: function(client, tabGrip, callback) {
    if (!client) {
      return;
    }

    if (!tabGrip) {
      return;
    }

    this.debuggerClient = client;
    this.targetFront = this.toolbox.target.activeTab;
    this.webConsoleClient = this.toolbox.target.activeConsole;

    this.tabWatcher = new TabWatcher(this.toolbox, this);
    this.tabWatcher.connect();
  },

  pageLoadBegin: function(response) {
    this.resetCollector();
  },

  resetCollector: function() {
    if (this.collector) {
      this.collector.stop();
    }

    // A page is about to be loaded, start collecting HTTP
    // data from events sent from the backend.
    this.collector = new HarCollector({
      webConsoleClient: this.webConsoleClient,
      debuggerClient: this.debuggerClient,
    });

    this.collector.start();
  },

  /**
   * A page is done loading, export collected data. Note that
   * some requests for additional page resources might be pending,
   * so export all after all has been properly received from the backend.
   *
   * This collector still works and collects any consequent HTTP
   * traffic (e.g. XHRs) happening after the page is loaded and
   * The additional traffic can be exported by executing
   * triggerExport on this object.
   */
  pageLoadDone: function(response) {
    trace.log("HarAutomation.pageLoadDone; ", response);

    if (this.collector) {
      this.collector.waitForHarLoad().then(collector => {
        return this.autoExport();
      });
    }
  },

  autoExport: function() {
    const autoExport = Services.prefs.getBoolPref(prefDomain +
      "enableAutoExportToFile");

    if (!autoExport) {
      return Promise.resolve();
    }

    // Auto export to file is enabled, so save collected data
    // into a file and use all the default options.
    const data = {
      fileName: Services.prefs.getCharPref(prefDomain + "defaultFileName"),
    };

    return this.executeExport(data);
  },

  // Public API

  /**
   * Export all what is currently collected.
   */
  triggerExport: function(data) {
    if (!data.fileName) {
      data.fileName = Services.prefs.getCharPref(prefDomain +
        "defaultFileName");
    }

    return this.executeExport(data);
  },

  /**
   * Clear currently collected data.
   */
  clear: function() {
    this.resetCollector();
  },

  // HAR Export

  /**
   * Execute HAR export. This method fetches all data from the
   * Network panel (asynchronously) and saves it into a file.
   */
  executeExport: function(data) {
    const items = this.collector.getItems();
    const form = this.toolbox.target.form;
    const title = form.title || form.url;

    const options = {
      requestData: null,
      getTimingMarker: null,
      getString: this.getString.bind(this),
      view: this,
      items: items,
    };

    options.defaultFileName = data.fileName;
    options.compress = data.compress;
    options.title = data.title || title;
    options.id = data.id;
    options.jsonp = data.jsonp;
    options.includeResponseBodies = data.includeResponseBodies;
    options.jsonpCallback = data.jsonpCallback;
    options.forceExport = data.forceExport;

    trace.log("HarAutomation.executeExport; " + data.fileName, options);

    return HarExporter.fetchHarData(options).then(jsonString => {
      // Save the HAR file if the file name is provided.
      if (jsonString && options.defaultFileName) {
        const file = getDefaultTargetFile(options);
        if (file) {
          HarUtils.saveToFile(file, jsonString, options.compress);
        }
      }

      return jsonString;
    });
  },

  /**
   * Fetches the full text of a string.
   */
  getString: function(stringGrip) {
    return this.webConsoleClient.getString(stringGrip);
  },
};

// Helpers

function TabWatcher(toolbox, listener) {
  this.target = toolbox.target;
  this.listener = listener;

  this.onTabNavigated = this.onTabNavigated.bind(this);
}

TabWatcher.prototype = {
  // Connection

  connect: function() {
    this.target.on("navigate", this.onTabNavigated);
    this.target.on("will-navigate", this.onTabNavigated);
  },

  disconnect: function() {
    if (!this.target) {
      return;
    }

    this.target.off("navigate", this.onTabNavigated);
    this.target.off("will-navigate", this.onTabNavigated);
  },

  // Event Handlers

  /**
   * Called for each location change in the monitored tab.
   *
   * @param string aType
   *        Packet type.
   * @param object aPacket
   *        Packet received from the server.
   */
  onTabNavigated: function(type, packet) {
    switch (type) {
      case "will-navigate": {
        this.listener.pageLoadBegin(packet);
        break;
      }
      case "navigate": {
        this.listener.pageLoadDone(packet);
        break;
      }
    }
  },
};

// Protocol Helpers

/**
 * Returns target file for exported HAR data.
 */
function getDefaultTargetFile(options) {
  const path = options.defaultLogDir ||
    Services.prefs.getCharPref("devtools.netmonitor.har.defaultLogDir");
  const folder = HarUtils.getLocalDirectory(path);
  const fileName = HarUtils.getHarFileName(options.defaultFileName,
    options.jsonp, options.compress);

  folder.append(fileName);
  folder.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));

  return folder;
}

// Exports from this module
exports.HarAutomation = HarAutomation;
