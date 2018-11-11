/* Copyright 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

var EXPORTED_SYMBOLS = ["PdfJs"];

const PREF_PREFIX = "pdfjs";
const PREF_DISABLED = PREF_PREFIX + ".disabled";
const PREF_MIGRATION_VERSION = PREF_PREFIX + ".migrationVersion";
const PREF_PREVIOUS_ACTION = PREF_PREFIX + ".previousHandler.preferredAction";
const PREF_PREVIOUS_ASK = PREF_PREFIX +
                          ".previousHandler.alwaysAskBeforeHandling";
const PREF_DISABLED_PLUGIN_TYPES = "plugin.disable_full_page_plugin_for_types";
const PREF_ENABLED_CACHE_STATE = PREF_PREFIX + ".enabledCache.state";
const PREF_ENABLED_CACHE_INITIALIZED = PREF_PREFIX +
                                       ".enabledCache.initialized";
const PREF_APP_UPDATE_POSTUPDATE = "app.update.postupdate";
const TOPIC_PDFJS_HANDLER_CHANGED = "pdfjs:handlerChanged";
const TOPIC_PLUGINS_LIST_UPDATED = "plugins-list-updated";
const TOPIC_PLUGIN_INFO_UPDATED = "plugin-info-updated";
const PDF_CONTENT_TYPE = "application/pdf";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var Svc = {};
XPCOMUtils.defineLazyServiceGetter(Svc, "mime",
                                   "@mozilla.org/mime;1",
                                   "nsIMIMEService");
XPCOMUtils.defineLazyServiceGetter(Svc, "pluginHost",
                                   "@mozilla.org/plugin/host;1",
                                   "nsIPluginHost");
ChromeUtils.defineModuleGetter(this, "PdfjsChromeUtils",
                               "resource://pdf.js/PdfjsChromeUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PdfJsDefaultPreferences",
  "resource://pdf.js/PdfJsDefaultPreferences.jsm");

function getBoolPref(aPref, aDefaultValue) {
  try {
    return Services.prefs.getBoolPref(aPref);
  } catch (ex) {
    return aDefaultValue;
  }
}

function getIntPref(aPref, aDefaultValue) {
  try {
    return Services.prefs.getIntPref(aPref);
  } catch (ex) {
    return aDefaultValue;
  }
}

function isDefaultHandler() {
  if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
    throw new Error("isDefaultHandler should only get called in the parent " +
                    "process.");
  }
  return PdfjsChromeUtils.isDefaultHandlerApp();
}

function initializeDefaultPreferences() {
  var defaultBranch = Services.prefs.getDefaultBranch(PREF_PREFIX + ".");
  var defaultValue;
  for (var key in PdfJsDefaultPreferences) {
    defaultValue = PdfJsDefaultPreferences[key];
    switch (typeof defaultValue) {
      case "boolean":
        defaultBranch.setBoolPref(key, defaultValue);
        break;
      case "number":
        defaultBranch.setIntPref(key, defaultValue);
        break;
      case "string":
        defaultBranch.setCharPref(key, defaultValue);
        break;
    }
  }
}

var PdfJs = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
  _initialized: false,

  init: function init(remote) {
    if (Services.appinfo.processType !==
        Services.appinfo.PROCESS_TYPE_DEFAULT) {
      throw new Error("PdfJs.init should only get called " +
                      "in the parent process.");
    }
    PdfjsChromeUtils.init();
    this.initPrefs();

    Services.ppmm.sharedData.set("pdfjs.enabled", this.checkEnabled());
  },

  earlyInit() {
    // Note: Please keep this in sync with the duplicated logic in
    // nsBrowserGlue.js.
    Services.ppmm.sharedData.set("pdfjs.enabled", this.checkEnabled());
  },

  initPrefs: function initPrefs() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    if (!getBoolPref(PREF_DISABLED, true)) {
      this._migrate();
    }

    // Listen for when pdf.js is completely disabled or a different pdf handler
    // is chosen.
    Services.prefs.addObserver(PREF_DISABLED, this);
    Services.prefs.addObserver(PREF_DISABLED_PLUGIN_TYPES, this);
    Services.obs.addObserver(this, TOPIC_PDFJS_HANDLER_CHANGED);
    Services.obs.addObserver(this, TOPIC_PLUGINS_LIST_UPDATED);
    Services.obs.addObserver(this, TOPIC_PLUGIN_INFO_UPDATED);

    initializeDefaultPreferences();
  },

  uninit: function uninit() {
    if (this._initialized) {
      Services.prefs.removeObserver(PREF_DISABLED, this);
      Services.prefs.removeObserver(PREF_DISABLED_PLUGIN_TYPES, this);
      Services.obs.removeObserver(this, TOPIC_PDFJS_HANDLER_CHANGED);
      Services.obs.removeObserver(this, TOPIC_PLUGINS_LIST_UPDATED);
      Services.obs.removeObserver(this, TOPIC_PLUGIN_INFO_UPDATED);
      this._initialized = false;
    }
  },

  _migrate: function migrate() {
    const VERSION = 2;
    var currentVersion = getIntPref(PREF_MIGRATION_VERSION, 0);
    if (currentVersion >= VERSION) {
      return;
    }
    // Make pdf.js the default pdf viewer on the first migration.
    if (currentVersion < 1) {
      this._becomeHandler();
    }
    if (currentVersion < 2) {
      // cleaning up of unused database preference (see #3994)
      Services.prefs.clearUserPref(PREF_PREFIX + ".database");
    }
    Services.prefs.setIntPref(PREF_MIGRATION_VERSION, VERSION);
  },

  _becomeHandler: function _becomeHandler() {
    let handlerInfo = Svc.mime.getFromTypeAndExtension(PDF_CONTENT_TYPE, "pdf");
    let prefs = Services.prefs;
    if (handlerInfo.preferredAction !== Ci.nsIHandlerInfo.handleInternally &&
        handlerInfo.preferredAction !== false) {
      // Store the previous settings of preferredAction and
      // alwaysAskBeforeHandling in case we need to revert them in a hotfix that
      // would turn pdf.js off.
      prefs.setIntPref(PREF_PREVIOUS_ACTION, handlerInfo.preferredAction);
      prefs.setBoolPref(PREF_PREVIOUS_ASK, handlerInfo.alwaysAskBeforeHandling);
    }

    let handlerService = Cc["@mozilla.org/uriloader/handler-service;1"].
                         getService(Ci.nsIHandlerService);

    // Change and save mime handler settings.
    handlerInfo.alwaysAskBeforeHandling = false;
    handlerInfo.preferredAction = Ci.nsIHandlerInfo.handleInternally;
    handlerService.store(handlerInfo);

    // Also disable any plugins for pdfs.
    var stringTypes = "";
    var types = [];
    if (prefs.prefHasUserValue(PREF_DISABLED_PLUGIN_TYPES)) {
      stringTypes = prefs.getCharPref(PREF_DISABLED_PLUGIN_TYPES);
    }
    if (stringTypes !== "") {
      types = stringTypes.split(",");
    }

    if (!types.includes(PDF_CONTENT_TYPE)) {
      types.push(PDF_CONTENT_TYPE);
    }
    prefs.setCharPref(PREF_DISABLED_PLUGIN_TYPES, types.join(","));

    // Update the category manager in case the plugins are already loaded.
    Services.catMan.deleteCategoryEntry("Gecko-Content-Viewers",
                                        PDF_CONTENT_TYPE,
                                        false);
  },

  _isEnabled: function _isEnabled() {
    var disabled = getBoolPref(PREF_DISABLED, true);
    if (disabled) {
      return false;
    }

    // Check if the 'application/pdf' preview handler is configured properly.
    if (!isDefaultHandler()) {
      return false;
    }

    // Check if we have disabled plugin handling of 'application/pdf' in prefs
    if (Services.prefs.prefHasUserValue(PREF_DISABLED_PLUGIN_TYPES)) {
      let disabledPluginTypes =
        Services.prefs.getCharPref(PREF_DISABLED_PLUGIN_TYPES).split(",");
      if (disabledPluginTypes.includes(PDF_CONTENT_TYPE)) {
        return true;
      }
    }

    // Check if there is an enabled pdf plugin.
    // Note: this check is performed last because getPluginTags() triggers
    // costly plugin list initialization (bug 881575)
    let tags = Cc["@mozilla.org/plugin/host;1"].
                  getService(Ci.nsIPluginHost).
                  getPluginTags();
    let enabledPluginFound = tags.some(function(tag) {
      if (tag.disabled) {
        return false;
      }
      let mimeTypes = tag.getMimeTypes();
      return mimeTypes.some(function(mimeType) {
        return mimeType === PDF_CONTENT_TYPE;
      });
    });

    // Use pdf.js if pdf plugin is not present or disabled
    return !enabledPluginFound;
  },

  checkEnabled: function checkEnabled() {
    let isEnabled = this._isEnabled();
    // This will be updated any time we observe a dependency changing, since
    // updateRegistration internally calls enabled.
    Services.prefs.setBoolPref(PREF_ENABLED_CACHE_STATE, isEnabled);
    return isEnabled;
  },

  // nsIObserver
  observe: function observe(aSubject, aTopic, aData) {
    if (Services.appinfo.processType !==
        Services.appinfo.PROCESS_TYPE_DEFAULT) {
      throw new Error("Only the parent process should be observing PDF " +
                      "handler changes.");
    }

    Services.ppmm.sharedData.set("pdfjs.enabled", this.checkEnabled());
  },

  /**
   * pdf.js is only enabled if it is both selected as the pdf viewer and if the
   * global switch enabling it is true.
   * @return {boolean} Whether or not it's enabled.
   */
  get enabled() {
    if (!Services.prefs.getBoolPref(PREF_ENABLED_CACHE_INITIALIZED, false)) {
      // If we just updated, and the cache hasn't been initialized, then we
      // can't assume a default state, and need to synchronously initialize
      // PdfJs
      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_POSTUPDATE)) {
        this.checkEnabled();
      }

      Services.prefs.setBoolPref(PREF_ENABLED_CACHE_INITIALIZED, true);
    }
    return Services.prefs.getBoolPref(PREF_ENABLED_CACHE_STATE, true);
  },
};

