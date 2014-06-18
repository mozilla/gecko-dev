/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// Cannot use Services.appinfo here, or else xpcshell-tests will blow up, as
// most tests later register different nsIAppInfo implementations, which
// wouldn't be reflected in Services.appinfo anymore, as the lazy getter
// underlying it would have been initialized if we used it here.
if ("@mozilla.org/xre/app-info;1" in Cc) {
  let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
  if (runtime.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // Refuse to run in child processes.
    throw new Error("You cannot use the AddonManager in child processes!");
  }
}


const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";
const PREF_DEFAULT_PROVIDERS_ENABLED  = "extensions.defaultProviders.enabled";
const PREF_EM_UPDATE_ENABLED          = "extensions.update.enabled";
const PREF_EM_LAST_APP_VERSION        = "extensions.lastAppVersion";
const PREF_EM_LAST_PLATFORM_VERSION   = "extensions.lastPlatformVersion";
const PREF_EM_AUTOUPDATE_DEFAULT      = "extensions.update.autoUpdateDefault";
const PREF_EM_STRICT_COMPATIBILITY    = "extensions.strictCompatibility";
const PREF_EM_CHECK_UPDATE_SECURITY   = "extensions.checkUpdateSecurity";
const PREF_EM_UPDATE_BACKGROUND_URL   = "extensions.update.background.url";
const PREF_APP_UPDATE_ENABLED         = "app.update.enabled";
const PREF_APP_UPDATE_AUTO            = "app.update.auto";
const PREF_EM_HOTFIX_ID               = "extensions.hotfix.id";
const PREF_EM_HOTFIX_LASTVERSION      = "extensions.hotfix.lastVersion";
const PREF_EM_HOTFIX_URL              = "extensions.hotfix.url";
const PREF_EM_CERT_CHECKATTRIBUTES    = "extensions.hotfix.cert.checkAttributes";
const PREF_EM_HOTFIX_CERTS            = "extensions.hotfix.certs.";
const PREF_MATCH_OS_LOCALE            = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE            = "general.useragent.locale";
const UNKNOWN_XPCOM_ABI               = "unknownABI";

const UPDATE_REQUEST_VERSION          = 2;
const CATEGORY_UPDATE_PARAMS          = "extension-update-params";

const XMLURI_BLOCKLIST                = "http://www.mozilla.org/2006/addons-blocklist";

const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const FILE_BLOCKLIST                  = "blocklist.xml";

const BRANCH_REGEXP                   = /^([^\.]+\.[0-9]+[a-z]*).*/gi;
const PREF_EM_CHECK_COMPATIBILITY_BASE = "extensions.checkCompatibility";
#ifdef MOZ_COMPATIBILITY_NIGHTLY
var PREF_EM_CHECK_COMPATIBILITY = PREF_EM_CHECK_COMPATIBILITY_BASE + ".nightly";
#else
var PREF_EM_CHECK_COMPATIBILITY;
#endif

const TOOLKIT_ID                      = "toolkit@mozilla.org";

const VALID_TYPES_REGEXP = /^[\w\-]+$/;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/addons/AddonRepository.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "CertUtils", function certUtilsLazyGetter() {
  let certUtils = {};
  Components.utils.import("resource://gre/modules/CertUtils.jsm", certUtils);
  return certUtils;
});


this.EXPORTED_SYMBOLS = [ "AddonManager", "AddonManagerPrivate" ];

const CATEGORY_PROVIDER_MODULE = "addon-provider-module";

// A list of providers to load by default
const DEFAULT_PROVIDERS = [
  "resource://gre/modules/addons/XPIProvider.jsm",
  "resource://gre/modules/LightweightThemeManager.jsm"
];

Cu.import("resource://gre/modules/Log.jsm");
// Configure a logger at the parent 'addons' level to format
// messages for all the modules under addons.*
const PARENT_LOGGER_ID = "addons";
let parentLogger = Log.repository.getLogger(PARENT_LOGGER_ID);
parentLogger.level = Log.Level.Warn;
let formatter = new Log.BasicFormatter();
// Set parent logger (and its children) to append to
// the Javascript section of the Browser Console
parentLogger.addAppender(new Log.ConsoleAppender(formatter));
// Set parent logger (and its children) to
// also append to standard out
parentLogger.addAppender(new Log.DumpAppender(formatter));

// Create a new logger (child of 'addons' logger)
// for use by the Addons Manager
const LOGGER_ID = "addons.manager";
let logger = Log.repository.getLogger(LOGGER_ID);

// Provide the ability to enable/disable logging
// messages at runtime.
// If the "extensions.logging.enabled" preference is
// missing or 'false', messages at the WARNING and higher
// severity should be logged to the JS console and standard error.
// If "extensions.logging.enabled" is set to 'true', messages
// at DEBUG and higher should go to JS console and standard error.
const PREF_LOGGING_ENABLED = "extensions.logging.enabled";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

/**
 * Preference listener which listens for a change in the
 * "extensions.logging.enabled" preference and changes the logging level of the
 * parent 'addons' level logger accordingly.
 */
var PrefObserver = {
    init: function PrefObserver_init() {
      Services.prefs.addObserver(PREF_LOGGING_ENABLED, this, false);
      Services.obs.addObserver(this, "xpcom-shutdown", false);
      this.observe(null, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, PREF_LOGGING_ENABLED);
    },

    observe: function PrefObserver_observe(aSubject, aTopic, aData) {
      if (aTopic == "xpcom-shutdown") {
        Services.prefs.removeObserver(PREF_LOGGING_ENABLED, this);
        Services.obs.removeObserver(this, "xpcom-shutdown");
      }
      else if (aTopic == NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) {
        let debugLogEnabled = false;
        try {
          debugLogEnabled = Services.prefs.getBoolPref(PREF_LOGGING_ENABLED);
        }
        catch (e) {
        }
        if (debugLogEnabled) {
          parentLogger.level = Log.Level.Debug;
        }
        else {
          parentLogger.level = Log.Level.Warn;
        }
      }
    }
};

PrefObserver.init();

/**
 * Calls a callback method consuming any thrown exception. Any parameters after
 * the callback parameter will be passed to the callback.
 *
 * @param  aCallback
 *         The callback method to call
 */
function safeCall(aCallback, ...aArgs) {
  try {
    aCallback.apply(null, aArgs);
  }
  catch (e) {
    logger.warn("Exception calling callback", e);
  }
}

/**
 * Calls a method on a provider if it exists and consumes any thrown exception.
 * Any parameters after the dflt parameter are passed to the provider's method.
 *
 * @param  aProvider
 *         The provider to call
 * @param  aMethod
 *         The method name to call
 * @param  aDefault
 *         A default return value if the provider does not implement the named
 *         method or throws an error.
 * @return the return value from the provider or dflt if the provider does not
 *         implement method or throws an error
 */
function callProvider(aProvider, aMethod, aDefault, ...aArgs) {
  if (!(aMethod in aProvider))
    return aDefault;

  try {
    return aProvider[aMethod].apply(aProvider, aArgs);
  }
  catch (e) {
    logger.error("Exception calling provider " + aMethod, e);
    return aDefault;
  }
}

/**
 * Gets the currently selected locale for display.
 * @return  the selected locale or "en-US" if none is selected
 */
function getLocale() {
  try {
    if (Services.prefs.getBoolPref(PREF_MATCH_OS_LOCALE))
      return Services.locale.getLocaleComponentForUserAgent();
  }
  catch (e) { }

  try {
    let locale = Services.prefs.getComplexValue(PREF_SELECTED_LOCALE,
                                                Ci.nsIPrefLocalizedString);
    if (locale)
      return locale;
  }
  catch (e) { }

  try {
    return Services.prefs.getCharPref(PREF_SELECTED_LOCALE);
  }
  catch (e) { }

  return "en-US";
}

/**
 * A helper class to repeatedly call a listener with each object in an array
 * optionally checking whether the object has a method in it.
 *
 * @param  aObjects
 *         The array of objects to iterate through
 * @param  aMethod
 *         An optional method name, if not null any objects without this method
 *         will not be passed to the listener
 * @param  aListener
 *         A listener implementing nextObject and noMoreObjects methods. The
 *         former will be called with the AsyncObjectCaller as the first
 *         parameter and the object as the second. noMoreObjects will be passed
 *         just the AsyncObjectCaller
 */
function AsyncObjectCaller(aObjects, aMethod, aListener) {
  this.objects = aObjects.slice(0);
  this.method = aMethod;
  this.listener = aListener;

  this.callNext();
}

AsyncObjectCaller.prototype = {
  objects: null,
  method: null,
  listener: null,

  /**
   * Passes the next object to the listener or calls noMoreObjects if there
   * are none left.
   */
  callNext: function AOC_callNext() {
    if (this.objects.length == 0) {
      this.listener.noMoreObjects(this);
      return;
    }

    let object = this.objects.shift();
    if (!this.method || this.method in object)
      this.listener.nextObject(this, object);
    else
      this.callNext();
  }
};

/**
 * This represents an author of an add-on (e.g. creator or developer)
 *
 * @param  aName
 *         The name of the author
 * @param  aURL
 *         The URL of the author's profile page
 */
function AddonAuthor(aName, aURL) {
  this.name = aName;
  this.url = aURL;
}

AddonAuthor.prototype = {
  name: null,
  url: null,

  // Returns the author's name, defaulting to the empty string
  toString: function AddonAuthor_toString() {
    return this.name || "";
  }
}

/**
 * This represents an screenshot for an add-on
 *
 * @param  aURL
 *         The URL to the full version of the screenshot
 * @param  aWidth
 *         The width in pixels of the screenshot
 * @param  aHeight
 *         The height in pixels of the screenshot
 * @param  aThumbnailURL
 *         The URL to the thumbnail version of the screenshot
 * @param  aThumbnailWidth
 *         The width in pixels of the thumbnail version of the screenshot
 * @param  aThumbnailHeight
 *         The height in pixels of the thumbnail version of the screenshot
 * @param  aCaption
 *         The caption of the screenshot
 */
function AddonScreenshot(aURL, aWidth, aHeight, aThumbnailURL,
                         aThumbnailWidth, aThumbnailHeight, aCaption) {
  this.url = aURL;
  if (aWidth) this.width = aWidth;
  if (aHeight) this.height = aHeight;
  if (aThumbnailURL) this.thumbnailURL = aThumbnailURL;
  if (aThumbnailWidth) this.thumbnailWidth = aThumbnailWidth;
  if (aThumbnailHeight) this.thumbnailHeight = aThumbnailHeight;
  if (aCaption) this.caption = aCaption;
}

AddonScreenshot.prototype = {
  url: null,
  width: null,
  height: null,
  thumbnailURL: null,
  thumbnailWidth: null,
  thumbnailHeight: null,
  caption: null,

  // Returns the screenshot URL, defaulting to the empty string
  toString: function AddonScreenshot_toString() {
    return this.url || "";
  }
}


/**
 * This represents a compatibility override for an addon.
 *
 * @param  aType
 *         Overrride type - "compatible" or "incompatible"
 * @param  aMinVersion
 *         Minimum version of the addon to match
 * @param  aMaxVersion
 *         Maximum version of the addon to match
 * @param  aAppID
 *         Application ID used to match appMinVersion and appMaxVersion
 * @param  aAppMinVersion
 *         Minimum version of the application to match
 * @param  aAppMaxVersion
 *         Maximum version of the application to match
 */
function AddonCompatibilityOverride(aType, aMinVersion, aMaxVersion, aAppID,
                                    aAppMinVersion, aAppMaxVersion) {
  this.type = aType;
  this.minVersion = aMinVersion;
  this.maxVersion = aMaxVersion;
  this.appID = aAppID;
  this.appMinVersion = aAppMinVersion;
  this.appMaxVersion = aAppMaxVersion;
}

AddonCompatibilityOverride.prototype = {
  /**
   * Type of override - "incompatible" or "compatible".
   * Only "incompatible" is supported for now.
   */
  type: null,

  /**
   * Min version of the addon to match.
   */
  minVersion: null,

  /**
   * Max version of the addon to match.
   */
  maxVersion: null,

  /**
   * Application ID to match.
   */
  appID: null,

  /**
   * Min version of the application to match.
   */
  appMinVersion: null,

  /**
   * Max version of the application to match.
   */
  appMaxVersion: null
};


/**
 * A type of add-on, used by the UI to determine how to display different types
 * of add-ons.
 *
 * @param  aID
 *         The add-on type ID
 * @param  aLocaleURI
 *         The URI of a localized properties file to get the displayable name
 *         for the type from
 * @param  aLocaleKey
 *         The key for the string in the properties file or the actual display
 *         name if aLocaleURI is null. Include %ID% to include the type ID in
 *         the key
 * @param  aViewType
 *         The optional type of view to use in the UI
 * @param  aUIPriority
 *         The priority is used by the UI to list the types in order. Lower
 *         values push the type higher in the list.
 * @param  aFlags
 *         An option set of flags that customize the display of the add-on in
 *         the UI.
 */
function AddonType(aID, aLocaleURI, aLocaleKey, aViewType, aUIPriority, aFlags) {
  if (!aID)
    throw Components.Exception("An AddonType must have an ID", Cr.NS_ERROR_INVALID_ARG);

  if (aViewType && aUIPriority === undefined)
    throw Components.Exception("An AddonType with a defined view must have a set UI priority",
                               Cr.NS_ERROR_INVALID_ARG);

  if (!aLocaleKey)
    throw Components.Exception("An AddonType must have a displayable name",
                               Cr.NS_ERROR_INVALID_ARG);

  this.id = aID;
  this.uiPriority = aUIPriority;
  this.viewType = aViewType;
  this.flags = aFlags;

  if (aLocaleURI) {
    this.__defineGetter__("name", function nameGetter() {
      delete this.name;
      let bundle = Services.strings.createBundle(aLocaleURI);
      this.name = bundle.GetStringFromName(aLocaleKey.replace("%ID%", aID));
      return this.name;
    });
  }
  else {
    this.name = aLocaleKey;
  }
}

var gStarted = false;
var gStartupComplete = false;
var gCheckCompatibility = true;
var gStrictCompatibility = true;
var gCheckUpdateSecurityDefault = true;
var gCheckUpdateSecurity = gCheckUpdateSecurityDefault;
var gUpdateEnabled = true;
var gAutoUpdateDefault = true;
var gHotfixID = null;

/**
 * This is the real manager, kept here rather than in AddonManager to keep its
 * contents hidden from API users.
 */
var AddonManagerInternal = {
  managerListeners: [],
  installListeners: [],
  addonListeners: [],
  typeListeners: [],
  providers: [],
  types: {},
  startupChanges: {},
  // Store telemetry details per addon provider
  telemetryDetails: {},

  // A read-only wrapper around the types dictionary
  typesProxy: Proxy.create({
    getOwnPropertyDescriptor: function typesProxy_getOwnPropertyDescriptor(aName) {
      if (!(aName in AddonManagerInternal.types))
        return undefined;

      return {
        value: AddonManagerInternal.types[aName].type,
        writable: false,
        configurable: false,
        enumerable: true
      }
    },

    getPropertyDescriptor: function typesProxy_getPropertyDescriptor(aName) {
      return this.getOwnPropertyDescriptor(aName);
    },

    getOwnPropertyNames: function typesProxy_getOwnPropertyNames() {
      return Object.keys(AddonManagerInternal.types);
    },

    getPropertyNames: function typesProxy_getPropertyNames() {
      return this.getOwnPropertyNames();
    },

    delete: function typesProxy_delete(aName) {
      // Not allowed to delete properties
      return false;
    },

    defineProperty: function typesProxy_defineProperty(aName, aProperty) {
      // Ignore attempts to define properties
    },

    fix: function typesProxy_fix(){
      return undefined;
    },

    // Despite MDC's claims to the contrary, it is required that this trap
    // be defined
    enumerate: function typesProxy_enumerate() {
      // All properties are enumerable
      return this.getPropertyNames();
    }
  }),

  recordTimestamp: function AMI_recordTimestamp(name, value) {
    this.TelemetryTimestamps.add(name, value);
  },

  validateBlocklist: function AMI_validateBlocklist() {
    let appBlocklist = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);

    // If there is no application shipped blocklist then there is nothing to do
    if (!appBlocklist.exists())
      return;

    let profileBlocklist = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);

    // If there is no blocklist in the profile then copy the application shipped
    // one there
    if (!profileBlocklist.exists()) {
      try {
        appBlocklist.copyTo(profileBlocklist.parent, FILE_BLOCKLIST);
      }
      catch (e) {
        logger.warn("Failed to copy the application shipped blocklist to the profile", e);
      }
      return;
    }

    let fileStream = Cc["@mozilla.org/network/file-input-stream;1"].
                     createInstance(Ci.nsIFileInputStream);
    try {
      let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].
                    createInstance(Ci.nsIConverterInputStream);
      fileStream.init(appBlocklist, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
      cstream.init(fileStream, "UTF-8", 0, 0);

      let data = "";
      let str = {};
      let read = 0;
      do {
        read = cstream.readString(0xffffffff, str);
        data += str.value;
      } while (read != 0);

      let parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      var doc = parser.parseFromString(data, "text/xml");
    }
    catch (e) {
      logger.warn("Application shipped blocklist could not be loaded", e);
      return;
    }
    finally {
      try {
        fileStream.close();
      }
      catch (e) {
        logger.warn("Unable to close blocklist file stream", e);
      }
    }

    // If the namespace is incorrect then ignore the application shipped
    // blocklist
    if (doc.documentElement.namespaceURI != XMLURI_BLOCKLIST) {
      logger.warn("Application shipped blocklist has an unexpected namespace (" +
                  doc.documentElement.namespaceURI + ")");
      return;
    }

    // If there is no lastupdate information then ignore the application shipped
    // blocklist
    if (!doc.documentElement.hasAttribute("lastupdate"))
      return;

    // If the application shipped blocklist is older than the profile blocklist
    // then do nothing
    if (doc.documentElement.getAttribute("lastupdate") <=
        profileBlocklist.lastModifiedTime)
      return;

    // Otherwise copy the application shipped blocklist to the profile
    try {
      appBlocklist.copyTo(profileBlocklist.parent, FILE_BLOCKLIST);
    }
    catch (e) {
      logger.warn("Failed to copy the application shipped blocklist to the profile", e);
    }
  },

  /**
   * Initializes the AddonManager, loading any known providers and initializing
   * them.
   */
  startup: function AMI_startup() {
    try {
      if (gStarted)
        return;

      this.recordTimestamp("AMI_startup_begin");

      // clear this for xpcshell test restarts
      for (let provider in this.telemetryDetails)
        delete this.telemetryDetails[provider];

      let appChanged = undefined;

      let oldAppVersion = null;
      try {
        oldAppVersion = Services.prefs.getCharPref(PREF_EM_LAST_APP_VERSION);
        appChanged = Services.appinfo.version != oldAppVersion;
      }
      catch (e) { }

      let oldPlatformVersion = null;
      try {
        oldPlatformVersion = Services.prefs.getCharPref(PREF_EM_LAST_PLATFORM_VERSION);
      }
      catch (e) { }

      if (appChanged !== false) {
        logger.debug("Application has been upgraded");
        Services.prefs.setCharPref(PREF_EM_LAST_APP_VERSION,
                                   Services.appinfo.version);
        Services.prefs.setCharPref(PREF_EM_LAST_PLATFORM_VERSION,
                                   Services.appinfo.platformVersion);
        Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION,
                                  (appChanged === undefined ? 0 : -1));
        this.validateBlocklist();
      }

#ifndef MOZ_COMPATIBILITY_NIGHTLY
      PREF_EM_CHECK_COMPATIBILITY = PREF_EM_CHECK_COMPATIBILITY_BASE + "." +
                                    Services.appinfo.version.replace(BRANCH_REGEXP, "$1");
#endif

      try {
        gCheckCompatibility = Services.prefs.getBoolPref(PREF_EM_CHECK_COMPATIBILITY);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_CHECK_COMPATIBILITY, this, false);

      try {
        gStrictCompatibility = Services.prefs.getBoolPref(PREF_EM_STRICT_COMPATIBILITY);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_STRICT_COMPATIBILITY, this, false);

      try {
        let defaultBranch = Services.prefs.getDefaultBranch("");
        gCheckUpdateSecurityDefault = defaultBranch.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
      } catch(e) {}

      try {
        gCheckUpdateSecurity = Services.prefs.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_CHECK_UPDATE_SECURITY, this, false);

      try {
        gUpdateEnabled = Services.prefs.getBoolPref(PREF_EM_UPDATE_ENABLED);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_UPDATE_ENABLED, this, false);

      try {
        gAutoUpdateDefault = Services.prefs.getBoolPref(PREF_EM_AUTOUPDATE_DEFAULT);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_AUTOUPDATE_DEFAULT, this, false);

      try {
        gHotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_HOTFIX_ID, this, false);

      let defaultProvidersEnabled = true;
      try {
        defaultProvidersEnabled = Services.prefs.getBoolPref(PREF_DEFAULT_PROVIDERS_ENABLED);
      } catch (e) {}
      AddonManagerPrivate.recordSimpleMeasure("default_providers", defaultProvidersEnabled);

      // Ensure all default providers have had a chance to register themselves
      if (defaultProvidersEnabled) {
        for (let url of DEFAULT_PROVIDERS) {
          try {
            let scope = {};
            Components.utils.import(url, scope);
            // Sanity check - make sure the provider exports a symbol that
            // has a 'startup' method
            let syms = Object.keys(scope);
            if ((syms.length < 1) ||
                (typeof scope[syms[0]].startup != "function")) {
              logger.warn("Provider " + url + " has no startup()");
              AddonManagerPrivate.recordException("AMI", "provider " + url, "no startup()");
            }
            logger.debug("Loaded provider scope for " + url + ": " + Object.keys(scope).toSource());
          }
          catch (e) {
            AddonManagerPrivate.recordException("AMI", "provider " + url + " load failed", e);
            logger.error("Exception loading default provider \"" + url + "\"", e);
          }
        };
      }

      // Load any providers registered in the category manager
      let catman = Cc["@mozilla.org/categorymanager;1"].
                   getService(Ci.nsICategoryManager);
      let entries = catman.enumerateCategory(CATEGORY_PROVIDER_MODULE);
      while (entries.hasMoreElements()) {
        let entry = entries.getNext().QueryInterface(Ci.nsISupportsCString).data;
        let url = catman.getCategoryEntry(CATEGORY_PROVIDER_MODULE, entry);

        try {
          Components.utils.import(url, {});
        }
        catch (e) {
          AddonManagerPrivate.recordException("AMI", "provider " + url + " load failed", e);
          logger.error("Exception loading provider " + entry + " from category \"" +
                url + "\"", e);
        }
      }

      // Register our shutdown handler with the AsyncShutdown manager
      AsyncShutdown.profileBeforeChange.addBlocker("AddonManager: shutting down providers",
                                                   this.shutdown.bind(this));

      // Once we start calling providers we must allow all normal methods to work.
      gStarted = true;

      this.callProviders("startup", appChanged, oldAppVersion,
                         oldPlatformVersion);

      // If this is a new profile just pretend that there were no changes
      if (appChanged === undefined) {
        for (let type in this.startupChanges)
          delete this.startupChanges[type];
      }

      gStartupComplete = true;
      this.recordTimestamp("AMI_startup_end");
    }
    catch (e) {
      logger.error("startup failed", e);
      AddonManagerPrivate.recordException("AMI", "startup failed", e);
    }
  },

  /**
   * Registers a new AddonProvider.
   *
   * @param  aProvider
   *         The provider to register
   * @param  aTypes
   *         An optional array of add-on types
   */
  registerProvider: function AMI_registerProvider(aProvider, aTypes) {
    if (!aProvider || typeof aProvider != "object")
      throw Components.Exception("aProvider must be specified",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    this.providers.push(aProvider);

    if (aTypes) {
      aTypes.forEach(function(aType) {
        if (!(aType.id in this.types)) {
          if (!VALID_TYPES_REGEXP.test(aType.id)) {
            logger.warn("Ignoring invalid type " + aType.id);
            return;
          }

          this.types[aType.id] = {
            type: aType,
            providers: [aProvider]
          };

          let typeListeners = this.typeListeners.slice(0);
          for (let listener of typeListeners) {
            safeCall(function listenerSafeCall() {
              listener.onTypeAdded(aType);
            });
          }
        }
        else {
          this.types[aType.id].providers.push(aProvider);
        }
      }, this);
    }

    // If we're registering after startup call this provider's startup.
    if (gStarted)
      callProvider(aProvider, "startup");
  },

  /**
   * Unregisters an AddonProvider.
   *
   * @param  aProvider
   *         The provider to unregister
   */
  unregisterProvider: function AMI_unregisterProvider(aProvider) {
    if (!aProvider || typeof aProvider != "object")
      throw Components.Exception("aProvider must be specified",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.providers.length) {
      if (this.providers[pos] == aProvider)
        this.providers.splice(pos, 1);
      else
        pos++;
    }

    for (let type in this.types) {
      this.types[type].providers = this.types[type].providers.filter(function filterProvider(p) p != aProvider);
      if (this.types[type].providers.length == 0) {
        let oldType = this.types[type].type;
        delete this.types[type];

        let typeListeners = this.typeListeners.slice(0);
        for (let listener of typeListeners) {
          safeCall(function listenerSafeCall() {
            listener.onTypeRemoved(oldType);
          });
        }
      }
    }

    // If we're unregistering after startup call this provider's shutdown.
    if (gStarted)
      callProvider(aProvider, "shutdown");
  },

  /**
   * Calls a method on all registered providers if it exists and consumes any
   * thrown exception. Return values are ignored. Any parameters after the
   * method parameter are passed to the provider's method.
   *
   * @param  aMethod
   *         The method name to call
   * @see    callProvider
   */
  callProviders: function AMI_callProviders(aMethod, ...aArgs) {
    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let providers = this.providers.slice(0);
    for (let provider of providers) {
      try {
        if (aMethod in provider)
          provider[aMethod].apply(provider, aArgs);
      }
      catch (e) {
        AddonManagerPrivate.recordException("AMI", "provider " + aMethod, e);
        logger.error("Exception calling provider " + aMethod, e);
      }
    }
  },

  /**
   * Calls a method on all registered providers, if the provider implements
   * the method. The called method is expected to return a promise, and
   * callProvidersAsync returns a promise that resolves when every provider
   * method has either resolved or rejected. Rejection reasons are logged
   * but otherwise ignored. Return values are ignored. Any parameters after the
   * method parameter are passed to the provider's method.
   *
   * @param  aMethod
   *         The method name to call
   * @see    callProvider
   */
  callProvidersAsync: function AMI_callProviders(aMethod, ...aArgs) {
    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let allProviders = [];

    let providers = this.providers.slice(0);
    for (let provider of providers) {
      try {
        if (aMethod in provider) {
          // Resolve a new promise with the result of the method, to handle both
          // methods that return values (or nothing) and methods that return promises.
          let providerResult = provider[aMethod].apply(provider, aArgs);
          let nextPromise = Promise.resolve(providerResult);
          // Log and swallow the errors from methods that do return promises.
          nextPromise = nextPromise.then(
              null,
              e => logger.error("Exception calling provider " + aMethod, e));
          allProviders.push(nextPromise);
        }
      }
      catch (e) {
        logger.error("Exception calling provider " + aMethod, e);
      }
    }
    // Because we use promise.then to catch and log all errors above, Promise.all()
    // will never exit early because of a rejection.
    return Promise.all(allProviders);
  },

  /**
   * Shuts down the addon manager and all registered providers, this must clean
   * up everything in order for automated tests to fake restarts.
   * @return Promise{null} that resolves when all providers and dependent modules
   *                       have finished shutting down
   */
  shutdown: function AMI_shutdown() {
    logger.debug("shutdown");
    // Clean up listeners
    Services.prefs.removeObserver(PREF_EM_CHECK_COMPATIBILITY, this);
    Services.prefs.removeObserver(PREF_EM_STRICT_COMPATIBILITY, this);
    Services.prefs.removeObserver(PREF_EM_CHECK_UPDATE_SECURITY, this);
    Services.prefs.removeObserver(PREF_EM_UPDATE_ENABLED, this);
    Services.prefs.removeObserver(PREF_EM_AUTOUPDATE_DEFAULT, this);
    Services.prefs.removeObserver(PREF_EM_HOTFIX_ID, this);

    // Only shut down providers if they've been started. Shut down
    // AddonRepository after providers (if any).
    let shuttingDown = null;
    if (gStarted) {
      shuttingDown = this.callProvidersAsync("shutdown")
        .then(null,
              err => logger.error("Failure during async provider shutdown", err))
        .then(() => AddonRepository.shutdown());
    }
    else {
      shuttingDown = AddonRepository.shutdown();
    }

      shuttingDown.then(val => logger.debug("Async provider shutdown done"),
                        err => logger.error("Failure during AddonRepository shutdown", err))
      .then(() => {
        this.managerListeners.splice(0, this.managerListeners.length);
        this.installListeners.splice(0, this.installListeners.length);
        this.addonListeners.splice(0, this.addonListeners.length);
        this.typeListeners.splice(0, this.typeListeners.length);
        for (let type in this.startupChanges)
          delete this.startupChanges[type];
        gStarted = false;
        gStartupComplete = false;
      });
    return shuttingDown;
  },

  /**
   * Notified when a preference we're interested in has changed.
   *
   * @see nsIObserver
   */
  observe: function AMI_observe(aSubject, aTopic, aData) {
    switch (aData) {
      case PREF_EM_CHECK_COMPATIBILITY: {
        let oldValue = gCheckCompatibility;
        try {
          gCheckCompatibility = Services.prefs.getBoolPref(PREF_EM_CHECK_COMPATIBILITY);
        } catch(e) {
          gCheckCompatibility = true;
        }

        this.callManagerListeners("onCompatibilityModeChanged");

        if (gCheckCompatibility != oldValue)
          this.updateAddonAppDisabledStates();

        break;
      }
      case PREF_EM_STRICT_COMPATIBILITY: {
        let oldValue = gStrictCompatibility;
        try {
          gStrictCompatibility = Services.prefs.getBoolPref(PREF_EM_STRICT_COMPATIBILITY);
        } catch(e) {
          gStrictCompatibility = true;
        }

        this.callManagerListeners("onCompatibilityModeChanged");

        if (gStrictCompatibility != oldValue)
          this.updateAddonAppDisabledStates();

        break;
      }
      case PREF_EM_CHECK_UPDATE_SECURITY: {
        let oldValue = gCheckUpdateSecurity;
        try {
          gCheckUpdateSecurity = Services.prefs.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
        } catch(e) {
          gCheckUpdateSecurity = true;
        }

        this.callManagerListeners("onCheckUpdateSecurityChanged");

        if (gCheckUpdateSecurity != oldValue)
          this.updateAddonAppDisabledStates();

        break;
      }
      case PREF_EM_UPDATE_ENABLED: {
        let oldValue = gUpdateEnabled;
        try {
          gUpdateEnabled = Services.prefs.getBoolPref(PREF_EM_UPDATE_ENABLED);
        } catch(e) {
          gUpdateEnabled = true;
        }

        this.callManagerListeners("onUpdateModeChanged");
        break;
      }
      case PREF_EM_AUTOUPDATE_DEFAULT: {
        let oldValue = gAutoUpdateDefault;
        try {
          gAutoUpdateDefault = Services.prefs.getBoolPref(PREF_EM_AUTOUPDATE_DEFAULT);
        } catch(e) {
          gAutoUpdateDefault = true;
        }

        this.callManagerListeners("onUpdateModeChanged");
        break;
      }
      case PREF_EM_HOTFIX_ID: {
        try {
          gHotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
        } catch(e) {
          gHotfixID = null;
        }
        break;
      }
    }
  },

  /**
   * Replaces %...% strings in an addon url (update and updateInfo) with
   * appropriate values.
   *
   * @param  aAddon
   *         The Addon representing the add-on
   * @param  aUri
   *         The string representation of the URI to escape
   * @param  aAppVersion
   *         The optional application version to use for %APP_VERSION%
   * @return The appropriately escaped URI.
   */
  escapeAddonURI: function AMI_escapeAddonURI(aAddon, aUri, aAppVersion)
  {
    if (!aAddon || typeof aAddon != "object")
      throw Components.Exception("aAddon must be an Addon object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aUri || typeof aUri != "string")
      throw Components.Exception("aUri must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aAppVersion && typeof aAppVersion != "string")
      throw Components.Exception("aAppVersion must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    var addonStatus = aAddon.userDisabled || aAddon.softDisabled ? "userDisabled"
                                                                 : "userEnabled";

    if (!aAddon.isCompatible)
      addonStatus += ",incompatible";
    if (aAddon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
      addonStatus += ",blocklisted";
    if (aAddon.blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
      addonStatus += ",softblocked";

    try {
      var xpcomABI = Services.appinfo.XPCOMABI;
    } catch (ex) {
      xpcomABI = UNKNOWN_XPCOM_ABI;
    }

    let uri = aUri.replace(/%ITEM_ID%/g, aAddon.id);
    uri = uri.replace(/%ITEM_VERSION%/g, aAddon.version);
    uri = uri.replace(/%ITEM_STATUS%/g, addonStatus);
    uri = uri.replace(/%APP_ID%/g, Services.appinfo.ID);
    uri = uri.replace(/%APP_VERSION%/g, aAppVersion ? aAppVersion :
                                                      Services.appinfo.version);
    uri = uri.replace(/%REQ_VERSION%/g, UPDATE_REQUEST_VERSION);
    uri = uri.replace(/%APP_OS%/g, Services.appinfo.OS);
    uri = uri.replace(/%APP_ABI%/g, xpcomABI);
    uri = uri.replace(/%APP_LOCALE%/g, getLocale());
    uri = uri.replace(/%CURRENT_APP_VERSION%/g, Services.appinfo.version);

    // Replace custom parameters (names of custom parameters must have at
    // least 3 characters to prevent lookups for something like %D0%C8)
    var catMan = null;
    uri = uri.replace(/%(\w{3,})%/g, function parameterReplace(aMatch, aParam) {
      if (!catMan) {
        catMan = Cc["@mozilla.org/categorymanager;1"].
                 getService(Ci.nsICategoryManager);
      }

      try {
        var contractID = catMan.getCategoryEntry(CATEGORY_UPDATE_PARAMS, aParam);
        var paramHandler = Cc[contractID].getService(Ci.nsIPropertyBag2);
        return paramHandler.getPropertyAsAString(aParam);
      }
      catch(e) {
        return aMatch;
      }
    });

    // escape() does not properly encode + symbols in any embedded FVF strings.
    return uri.replace(/\+/g, "%2B");
  },

  /**
   * Performs a background update check by starting an update for all add-ons
   * that can be updated.
   * @return Promise{null} Resolves when the background update check is complete
   *                       (the resulting addon installations may still be in progress).
   */
  backgroundUpdateCheck: function AMI_backgroundUpdateCheck() {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    logger.debug("Background update check beginning");

    return Task.spawn(function* backgroundUpdateTask() {
      let hotfixID = this.hotfixID;

      let checkHotfix = hotfixID &&
                        Services.prefs.getBoolPref(PREF_APP_UPDATE_ENABLED) &&
                        Services.prefs.getBoolPref(PREF_APP_UPDATE_AUTO);

      if (!this.updateEnabled && !checkHotfix)
        return;

      Services.obs.notifyObservers(null, "addons-background-update-start", null);

      if (this.updateEnabled) {
        let scope = {};
        Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", scope);
        scope.LightweightThemeManager.updateCurrentTheme();

        let allAddons = yield new Promise((resolve, reject) => this.getAllAddons(resolve));

        // Repopulate repository cache first, to ensure compatibility overrides
        // are up to date before checking for addon updates.
        yield AddonRepository.backgroundUpdateCheck();

        // Keep track of all the async add-on updates happening in parallel
        let updates = [];

        for (let addon of allAddons) {
          if (addon.id == hotfixID) {
            continue;
          }

          // Check all add-ons for updates so that any compatibility updates will
          // be applied
          updates.push(new Promise((resolve, reject) => {
            addon.findUpdates({
              onUpdateAvailable: function BUC_onUpdateAvailable(aAddon, aInstall) {
                // Start installing updates when the add-on can be updated and
                // background updates should be applied.
                logger.debug("Found update for add-on ${id}", aAddon);
                if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE &&
                    AddonManager.shouldAutoUpdate(aAddon)) {
                  // XXX we really should resolve when this install is done,
                  // not when update-available check completes, no?
                  logger.debug("Starting install of ${id}", aAddon);
                  aInstall.install();
                }
              },

              onUpdateFinished: aAddon => { logger.debug("onUpdateFinished for ${id}", aAddon); resolve(); }
            }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
          }));
        }
        yield Promise.all(updates);
      }

      if (checkHotfix) {
        var hotfixVersion = "";
        try {
          hotfixVersion = Services.prefs.getCharPref(PREF_EM_HOTFIX_LASTVERSION);
        }
        catch (e) { }

        let url = null;
        if (Services.prefs.getPrefType(PREF_EM_HOTFIX_URL) == Ci.nsIPrefBranch.PREF_STRING)
          url = Services.prefs.getCharPref(PREF_EM_HOTFIX_URL);
        else
          url = Services.prefs.getCharPref(PREF_EM_UPDATE_BACKGROUND_URL);

        // Build the URI from a fake add-on data.
        url = AddonManager.escapeAddonURI({
          id: hotfixID,
          version: hotfixVersion,
          userDisabled: false,
          appDisabled: false
        }, url);

        Components.utils.import("resource://gre/modules/addons/AddonUpdateChecker.jsm");
        let update = null;
        try {
          let foundUpdates = yield new Promise((resolve, reject) => {
            AddonUpdateChecker.checkForUpdates(hotfixID, null, url, {
              onUpdateCheckComplete: resolve,
              onUpdateCheckError: reject
            });
          });
          update = AddonUpdateChecker.getNewestCompatibleUpdate(foundUpdates);
        } catch (e) {
          // AUC.checkForUpdates already logged the error
        }

        // Check that we have a hotfix update, and it's newer than the one we already
        // have installed (if any)
        if (update) {
          if (Services.vc.compare(hotfixVersion, update.version) < 0) {
            logger.debug("Downloading hotfix version " + update.version);
            let aInstall = yield new Promise((resolve, reject) =>
              AddonManager.getInstallForURL(update.updateURL, resolve,
                "application/x-xpinstall", update.updateHash, null,
                null, update.version));

            aInstall.addListener({
              onDownloadEnded: function BUC_onDownloadEnded(aInstall) {
                try {
                  if (!Services.prefs.getBoolPref(PREF_EM_CERT_CHECKATTRIBUTES))
                    return;
                }
                catch (e) {
                  // By default don't do certificate checks.
                  return;
                }

                try {
                  CertUtils.validateCert(aInstall.certificate,
                                         CertUtils.readCertPrefs(PREF_EM_HOTFIX_CERTS));
                }
                catch (e) {
                  logger.warn("The hotfix add-on was not signed by the expected " +
                       "certificate and so will not be installed.", e);
                  aInstall.cancel();
                }
              },

              onInstallEnded: function BUC_onInstallEnded(aInstall) {
                // Remember the last successfully installed version.
                Services.prefs.setCharPref(PREF_EM_HOTFIX_LASTVERSION,
                                           aInstall.version);
              },

              onInstallCancelled: function BUC_onInstallCancelled(aInstall) {
                // Revert to the previous version if the installation was
                // cancelled.
                Services.prefs.setCharPref(PREF_EM_HOTFIX_LASTVERSION,
                                           hotfixVersion);
              }
            });

            aInstall.install();
          }
        }
      }

      logger.debug("Background update check complete");
      Services.obs.notifyObservers(null,
                                   "addons-background-update-complete",
                                   null);
    }.bind(this));
  },

  /**
   * Adds a add-on to the list of detected changes for this startup. If
   * addStartupChange is called multiple times for the same add-on in the same
   * startup then only the most recent change will be remembered.
   *
   * @param  aType
   *         The type of change as a string. Providers can define their own
   *         types of changes or use the existing defined STARTUP_CHANGE_*
   *         constants
   * @param  aID
   *         The ID of the add-on
   */
  addStartupChange: function AMI_addStartupChange(aType, aID) {
    if (!aType || typeof aType != "string")
      throw Components.Exception("aType must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aID || typeof aID != "string")
      throw Components.Exception("aID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (gStartupComplete)
      return;

    // Ensure that an ID is only listed in one type of change
    for (let type in this.startupChanges)
      this.removeStartupChange(type, aID);

    if (!(aType in this.startupChanges))
      this.startupChanges[aType] = [];
    this.startupChanges[aType].push(aID);
  },

  /**
   * Removes a startup change for an add-on.
   *
   * @param  aType
   *         The type of change
   * @param  aID
   *         The ID of the add-on
   */
  removeStartupChange: function AMI_removeStartupChange(aType, aID) {
    if (!aType || typeof aType != "string")
      throw Components.Exception("aType must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aID || typeof aID != "string")
      throw Components.Exception("aID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (gStartupComplete)
      return;

    if (!(aType in this.startupChanges))
      return;

    this.startupChanges[aType] = this.startupChanges[aType].filter(
                                 function filterItem(aItem) aItem != aID);
  },

  /**
   * Calls all registered AddonManagerListeners with an event. Any parameters
   * after the method parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   */
  callManagerListeners: function AMI_callManagerListeners(aMethod, ...aArgs) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let managerListeners = this.managerListeners.slice(0);
    for (let listener of managerListeners) {
      try {
        if (aMethod in listener)
          listener[aMethod].apply(listener, aArgs);
      }
      catch (e) {
        logger.warn("AddonManagerListener threw exception when calling " + aMethod, e);
      }
    }
  },

  /**
   * Calls all registered InstallListeners with an event. Any parameters after
   * the extraListeners parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   * @param  aExtraListeners
   *         An optional array of extra InstallListeners to also call
   * @return false if any of the listeners returned false, true otherwise
   */
  callInstallListeners: function AMI_callInstallListeners(aMethod,
                                 aExtraListeners, ...aArgs) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aExtraListeners && !Array.isArray(aExtraListeners))
      throw Components.Exception("aExtraListeners must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    let result = true;
    let listeners;
    if (aExtraListeners)
      listeners = aExtraListeners.concat(this.installListeners);
    else
      listeners = this.installListeners.slice(0);

    for (let listener of listeners) {
      try {
        if (aMethod in listener) {
          if (listener[aMethod].apply(listener, aArgs) === false)
            result = false;
        }
      }
      catch (e) {
        logger.warn("InstallListener threw exception when calling " + aMethod, e);
      }
    }
    return result;
  },

  /**
   * Calls all registered AddonListeners with an event. Any parameters after
   * the method parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   */
  callAddonListeners: function AMI_callAddonListeners(aMethod, ...aArgs) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let addonListeners = this.addonListeners.slice(0);
    for (let listener of addonListeners) {
      try {
        if (aMethod in listener)
          listener[aMethod].apply(listener, aArgs);
      }
      catch (e) {
        logger.warn("AddonListener threw exception when calling " + aMethod, e);
      }
    }
  },

  /**
   * Notifies all providers that an add-on has been enabled when that type of
   * add-on only supports a single add-on being enabled at a time. This allows
   * the providers to disable theirs if necessary.
   *
   * @param  aID
   *         The ID of the enabled add-on
   * @param  aType
   *         The type of the enabled add-on
   * @param  aPendingRestart
   *         A boolean indicating if the change will only take place the next
   *         time the application is restarted
   */
  notifyAddonChanged: function AMI_notifyAddonChanged(aID, aType, aPendingRestart) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aID && typeof aID != "string")
      throw Components.Exception("aID must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aType || typeof aType != "string")
      throw Components.Exception("aType must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    this.callProviders("addonChanged", aID, aType, aPendingRestart);
  },

  /**
   * Notifies all providers they need to update the appDisabled property for
   * their add-ons in response to an application change such as a blocklist
   * update.
   */
  updateAddonAppDisabledStates: function AMI_updateAddonAppDisabledStates() {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    this.callProviders("updateAddonAppDisabledStates");
  },

  /**
   * Notifies all providers that the repository has updated its data for
   * installed add-ons.
   *
   * @param  aCallback
   *         Function to call when operation is complete.
   */
  updateAddonRepositoryData: function AMI_updateAddonRepositoryData(aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    new AsyncObjectCaller(this.providers, "updateAddonRepositoryData", {
      nextObject: function updateAddonRepositoryData_nextObject(aCaller, aProvider) {
        callProvider(aProvider,
                     "updateAddonRepositoryData",
                     null,
                     aCaller.callNext.bind(aCaller));
      },
      noMoreObjects: function updateAddonRepositoryData_noMoreObjects(aCaller) {
        safeCall(aCallback);
        // only tests should care about this
        Services.obs.notifyObservers(null, "TEST:addon-repository-data-updated", null);
      }
    });
  },

  /**
   * Asynchronously gets an AddonInstall for a URL.
   *
   * @param  aUrl
   *         The string represenation of the URL the add-on is located at
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   * @param  aMimetype
   *         The mimetype of the add-on
   * @param  aHash
   *         An optional hash of the add-on
   * @param  aName
   *         An optional placeholder name while the add-on is being downloaded
   * @param  aIcons
   *         Optional placeholder icons while the add-on is being downloaded
   * @param  aVersion
   *         An optional placeholder version while the add-on is being downloaded
   * @param  aLoadGroup
   *         An optional nsILoadGroup to associate any network requests with
   * @throws if the aUrl, aCallback or aMimetype arguments are not specified
   */
  getInstallForURL: function AMI_getInstallForURL(aUrl, aCallback, aMimetype,
                                                  aHash, aName, aIcons,
                                                  aVersion, aLoadGroup) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aUrl || typeof aUrl != "string")
      throw Components.Exception("aURL must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aHash && typeof aHash != "string")
      throw Components.Exception("aHash must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aName && typeof aName != "string")
      throw Components.Exception("aName must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aIcons) {
      if (typeof aIcons == "string")
        aIcons = { "32": aIcons };
      else if (typeof aIcons != "object")
        throw Components.Exception("aIcons must be a string, an object or null",
                                   Cr.NS_ERROR_INVALID_ARG);
    } else {
      aIcons = {};
    }

    if (aVersion && typeof aVersion != "string")
      throw Components.Exception("aVersion must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aLoadGroup && (!(aLoadGroup instanceof Ci.nsILoadGroup)))
      throw Components.Exception("aLoadGroup must be a nsILoadGroup or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    let providers = this.providers.slice(0);
    for (let provider of providers) {
      if (callProvider(provider, "supportsMimetype", false, aMimetype)) {
        callProvider(provider, "getInstallForURL", null,
                     aUrl, aHash, aName, aIcons, aVersion, aLoadGroup,
                     function  getInstallForURL_safeCall(aInstall) {
          safeCall(aCallback, aInstall);
        });
        return;
      }
    }
    safeCall(aCallback, null);
  },

  /**
   * Asynchronously gets an AddonInstall for an nsIFile.
   *
   * @param  aFile
   *         The nsIFile where the add-on is located
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   * @param  aMimetype
   *         An optional mimetype hint for the add-on
   * @throws if the aFile or aCallback arguments are not specified
   */
  getInstallForFile: function AMI_getInstallForFile(aFile, aCallback, aMimetype) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!(aFile instanceof Ci.nsIFile))
      throw Components.Exception("aFile must be a nsIFile",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aMimetype && typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    new AsyncObjectCaller(this.providers, "getInstallForFile", {
      nextObject: function getInstallForFile_nextObject(aCaller, aProvider) {
        callProvider(aProvider, "getInstallForFile", null, aFile,
                     function getInstallForFile_safeCall(aInstall) {
          if (aInstall)
            safeCall(aCallback, aInstall);
          else
            aCaller.callNext();
        });
      },

      noMoreObjects: function getInstallForFile_noMoreObjects(aCaller) {
        safeCall(aCallback, null);
      }
    });
  },

  /**
   * Asynchronously gets all current AddonInstalls optionally limiting to a list
   * of types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @param  aCallback
   *         A callback which will be passed an array of AddonInstalls
   * @throws If the aCallback argument is not specified
   */
  getInstallsByTypes: function AMI_getInstallsByTypes(aTypes, aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    let installs = [];

    new AsyncObjectCaller(this.providers, "getInstallsByTypes", {
      nextObject: function getInstallsByTypes_nextObject(aCaller, aProvider) {
        callProvider(aProvider, "getInstallsByTypes", null, aTypes,
                     function getInstallsByTypes_safeCall(aProviderInstalls) {
          installs = installs.concat(aProviderInstalls);
          aCaller.callNext();
        });
      },

      noMoreObjects: function getInstallsByTypes_noMoreObjects(aCaller) {
        safeCall(aCallback, installs);
      }
    });
  },

  /**
   * Asynchronously gets all current AddonInstalls.
   *
   * @param  aCallback
   *         A callback which will be passed an array of AddonInstalls
   */
  getAllInstalls: function AMI_getAllInstalls(aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    this.getInstallsByTypes(null, aCallback);
  },

  /**
   * Synchronously map a URI to the corresponding Addon ID.
   *
   * Mappable URIs are limited to in-application resources belonging to the
   * add-on, such as Javascript compartments, XUL windows, XBL bindings, etc.
   * but do not include URIs from meta data, such as the add-on homepage.
   *
   * @param  aURI
   *         nsIURI to map to an addon id
   * @return string containing the Addon ID or null
   * @see    amIAddonManager.mapURIToAddonID
   */
  mapURIToAddonID: function AMI_mapURIToAddonID(aURI) {
    if (!(aURI instanceof Ci.nsIURI)) {
      throw Components.Exception("aURI is not a nsIURI",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    // Try all providers
    let providers = this.providers.slice(0);
    for (let provider of providers) {
      var id = callProvider(provider, "mapURIToAddonID", null, aURI);
      if (id !== null) {
        return id;
      }
    }

    return null;
  },

  /**
   * Checks whether installation is enabled for a particular mimetype.
   *
   * @param  aMimetype
   *         The mimetype to check
   * @return true if installation is enabled for the mimetype
   */
  isInstallEnabled: function AMI_isInstallEnabled(aMimetype) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let providers = this.providers.slice(0);
    for (let provider of providers) {
      if (callProvider(provider, "supportsMimetype", false, aMimetype) &&
          callProvider(provider, "isInstallEnabled"))
        return true;
    }
    return false;
  },

  /**
   * Checks whether a particular source is allowed to install add-ons of a
   * given mimetype.
   *
   * @param  aMimetype
   *         The mimetype of the add-on
   * @param  aURI
   *         The optional nsIURI of the source
   * @return true if the source is allowed to install this mimetype
   */
  isInstallAllowed: function AMI_isInstallAllowed(aMimetype, aURI) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aURI && !(aURI instanceof Ci.nsIURI))
      throw Components.Exception("aURI must be a nsIURI or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    let providers = this.providers.slice(0);
    for (let provider of providers) {
      if (callProvider(provider, "supportsMimetype", false, aMimetype) &&
          callProvider(provider, "isInstallAllowed", null, aURI))
        return true;
    }
    return false;
  },

  /**
   * Starts installation of an array of AddonInstalls notifying the registered
   * web install listener of blocked or started installs.
   *
   * @param  aMimetype
   *         The mimetype of add-ons being installed
   * @param  aSource
   *         The optional nsIDOMWindow that started the installs
   * @param  aURI
   *         The optional nsIURI that started the installs
   * @param  aInstalls
   *         The array of AddonInstalls to be installed
   */
  installAddonsFromWebpage: function AMI_installAddonsFromWebpage(aMimetype,
                                                                  aSource,
                                                                  aURI,
                                                                  aInstalls) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aSource && !(aSource instanceof Ci.nsIDOMWindow))
      throw Components.Exception("aSource must be a nsIDOMWindow or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aURI && !(aURI instanceof Ci.nsIURI))
      throw Components.Exception("aURI must be a nsIURI or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!Array.isArray(aInstalls))
      throw Components.Exception("aInstalls must be an array",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!("@mozilla.org/addons/web-install-listener;1" in Cc)) {
      logger.warn("No web installer available, cancelling all installs");
      aInstalls.forEach(function(aInstall) {
        aInstall.cancel();
      });
      return;
    }

    try {
      let weblistener = Cc["@mozilla.org/addons/web-install-listener;1"].
                        getService(Ci.amIWebInstallListener);

      if (!this.isInstallEnabled(aMimetype, aURI)) {
        weblistener.onWebInstallDisabled(aSource, aURI, aInstalls,
                                         aInstalls.length);
      }
      else if (!this.isInstallAllowed(aMimetype, aURI)) {
        if (weblistener.onWebInstallBlocked(aSource, aURI, aInstalls,
                                            aInstalls.length)) {
          aInstalls.forEach(function(aInstall) {
            aInstall.install();
          });
        }
      }
      else if (weblistener.onWebInstallRequested(aSource, aURI, aInstalls,
                                                   aInstalls.length)) {
        aInstalls.forEach(function(aInstall) {
          aInstall.install();
        });
      }
    }
    catch (e) {
      // In the event that the weblistener throws during instantiation or when
      // calling onWebInstallBlocked or onWebInstallRequested all of the
      // installs should get cancelled.
      logger.warn("Failure calling web installer", e);
      aInstalls.forEach(function(aInstall) {
        aInstall.cancel();
      });
    }
  },

  /**
   * Adds a new InstallListener if the listener is not already registered.
   *
   * @param  aListener
   *         The InstallListener to add
   */
  addInstallListener: function AMI_addInstallListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a InstallListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.installListeners.some(function addInstallListener_matchListener(i) {
      return i == aListener; }))
      this.installListeners.push(aListener);
  },

  /**
   * Removes an InstallListener if the listener is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeInstallListener: function AMI_removeInstallListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a InstallListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.installListeners.length) {
      if (this.installListeners[pos] == aListener)
        this.installListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  /**
   * Asynchronously gets an add-on with a specific ID.
   *
   * @param  aID
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         The callback to pass the retrieved add-on to
   * @throws if the aID or aCallback arguments are not specified
   */
  getAddonByID: function AMI_getAddonByID(aID, aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aID || typeof aID != "string")
      throw Components.Exception("aID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    new AsyncObjectCaller(this.providers, "getAddonByID", {
      nextObject: function getAddonByID_nextObject(aCaller, aProvider) {
        callProvider(aProvider, "getAddonByID", null, aID,
                    function getAddonByID_safeCall(aAddon) {
          if (aAddon)
            safeCall(aCallback, aAddon);
          else
            aCaller.callNext();
        });
      },

      noMoreObjects: function getAddonByID_noMoreObjects(aCaller) {
        safeCall(aCallback, null);
      }
    });
  },

  /**
   * Asynchronously get an add-on with a specific Sync GUID.
   *
   * @param  aGUID
   *         String GUID of add-on to retrieve
   * @param  aCallback
   *         The callback to pass the retrieved add-on to.
   * @throws if the aGUID or aCallback arguments are not specified
   */
  getAddonBySyncGUID: function AMI_getAddonBySyncGUID(aGUID, aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aGUID || typeof aGUID != "string")
      throw Components.Exception("aGUID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    new AsyncObjectCaller(this.providers, "getAddonBySyncGUID", {
      nextObject: function getAddonBySyncGUID_nextObject(aCaller, aProvider) {
        callProvider(aProvider, "getAddonBySyncGUID", null, aGUID,
                    function getAddonBySyncGUID_safeCall(aAddon) {
          if (aAddon) {
            safeCall(aCallback, aAddon);
          } else {
            aCaller.callNext();
          }
        });
      },

      noMoreObjects: function getAddonBySyncGUID_noMoreObjects(aCaller) {
        safeCall(aCallback, null);
      }
    });
  },

  /**
   * Asynchronously gets an array of add-ons.
   *
   * @param  aIDs
   *         The array of IDs to retrieve
   * @param  aCallback
   *         The callback to pass an array of Addons to
   * @throws if the aID or aCallback arguments are not specified
   */
  getAddonsByIDs: function AMI_getAddonsByIDs(aIDs, aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!Array.isArray(aIDs))
      throw Components.Exception("aIDs must be an array",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    let addons = [];

    new AsyncObjectCaller(aIDs, null, {
      nextObject: function getAddonsByIDs_nextObject(aCaller, aID) {
        AddonManagerInternal.getAddonByID(aID,
                             function getAddonsByIDs_getAddonByID(aAddon) {
          addons.push(aAddon);
          aCaller.callNext();
        });
      },

      noMoreObjects: function getAddonsByIDs_noMoreObjects(aCaller) {
        safeCall(aCallback, addons);
      }
    });
  },

  /**
   * Asynchronously gets add-ons of specific types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @param  aCallback
   *         The callback to pass an array of Addons to.
   * @throws if the aCallback argument is not specified
   */
  getAddonsByTypes: function AMI_getAddonsByTypes(aTypes, aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    let addons = [];

    new AsyncObjectCaller(this.providers, "getAddonsByTypes", {
      nextObject: function getAddonsByTypes_nextObject(aCaller, aProvider) {
        callProvider(aProvider, "getAddonsByTypes", null, aTypes,
                     function getAddonsByTypes_concatAddons(aProviderAddons) {
          addons = addons.concat(aProviderAddons);
          aCaller.callNext();
        });
      },

      noMoreObjects: function getAddonsByTypes_noMoreObjects(aCaller) {
        safeCall(aCallback, addons);
      }
    });
  },

  /**
   * Asynchronously gets all installed add-ons.
   *
   * @param  aCallback
   *         A callback which will be passed an array of Addons
   */
  getAllAddons: function AMI_getAllAddons(aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    this.getAddonsByTypes(null, aCallback);
  },

  /**
   * Asynchronously gets add-ons that have operations waiting for an application
   * restart to complete.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @param  aCallback
   *         The callback to pass the array of Addons to
   * @throws if the aCallback argument is not specified
   */
  getAddonsWithOperationsByTypes:
  function AMI_getAddonsWithOperationsByTypes(aTypes, aCallback) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (typeof aCallback != "function")
      throw Components.Exception("aCallback must be a function",
                                 Cr.NS_ERROR_INVALID_ARG);

    let addons = [];

    new AsyncObjectCaller(this.providers, "getAddonsWithOperationsByTypes", {
      nextObject: function getAddonsWithOperationsByTypes_nextObject
                           (aCaller, aProvider) {
        callProvider(aProvider, "getAddonsWithOperationsByTypes", null, aTypes,
                     function getAddonsWithOperationsByTypes_concatAddons
                              (aProviderAddons) {
          addons = addons.concat(aProviderAddons);
          aCaller.callNext();
        });
      },

      noMoreObjects: function getAddonsWithOperationsByTypes_noMoreObjects(caller) {
        safeCall(aCallback, addons);
      }
    });
  },

  /**
   * Adds a new AddonManagerListener if the listener is not already registered.
   *
   * @param  aListener
   *         The listener to add
   */
  addManagerListener: function AMI_addManagerListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonManagerListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.managerListeners.some(function addManagerListener_matchListener(i) {
      return i == aListener; }))
      this.managerListeners.push(aListener);
  },

  /**
   * Removes an AddonManagerListener if the listener is registered.
   *
   * @param  aListener
   *         The listener to remove
   */
  removeManagerListener: function AMI_removeManagerListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonManagerListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.managerListeners.length) {
      if (this.managerListeners[pos] == aListener)
        this.managerListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  /**
   * Adds a new AddonListener if the listener is not already registered.
   *
   * @param  aListener
   *         The AddonListener to add
   */
  addAddonListener: function AMI_addAddonListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.addonListeners.some(function addAddonListener_matchListener(i) {
      return i == aListener; }))
      this.addonListeners.push(aListener);
  },

  /**
   * Removes an AddonListener if the listener is registered.
   *
   * @param  aListener
   *         The AddonListener to remove
   */
  removeAddonListener: function AMI_removeAddonListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.addonListeners.length) {
      if (this.addonListeners[pos] == aListener)
        this.addonListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  /**
   * Adds a new TypeListener if the listener is not already registered.
   *
   * @param  aListener
   *         The TypeListener to add
   */
  addTypeListener: function AMI_addTypeListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a TypeListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.typeListeners.some(function addTypeListener_matchListener(i) {
      return i == aListener; }))
      this.typeListeners.push(aListener);
  },

  /**
   * Removes an TypeListener if the listener is registered.
   *
   * @param  aListener
   *         The TypeListener to remove
   */
  removeTypeListener: function AMI_removeTypeListener(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a TypeListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.typeListeners.length) {
      if (this.typeListeners[pos] == aListener)
        this.typeListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  get addonTypes() {
    return this.typesProxy;
  },

  get autoUpdateDefault() {
    return gAutoUpdateDefault;
  },

  set autoUpdateDefault(aValue) {
    aValue = !!aValue;
    if (aValue != gAutoUpdateDefault)
      Services.prefs.setBoolPref(PREF_EM_AUTOUPDATE_DEFAULT, aValue);
    return aValue;
  },

  get checkCompatibility() {
    return gCheckCompatibility;
  },

  set checkCompatibility(aValue) {
    aValue = !!aValue;
    if (aValue != gCheckCompatibility) {
      if (!aValue)
        Services.prefs.setBoolPref(PREF_EM_CHECK_COMPATIBILITY, false);
      else
        Services.prefs.clearUserPref(PREF_EM_CHECK_COMPATIBILITY);
    }
    return aValue;
  },

  get strictCompatibility() {
    return gStrictCompatibility;
  },

  set strictCompatibility(aValue) {
    aValue = !!aValue;
    if (aValue != gStrictCompatibility)
      Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, aValue);
    return aValue;
  },

  get checkUpdateSecurityDefault() {
    return gCheckUpdateSecurityDefault;
  },

  get checkUpdateSecurity() {
    return gCheckUpdateSecurity;
  },

  set checkUpdateSecurity(aValue) {
    aValue = !!aValue;
    if (aValue != gCheckUpdateSecurity) {
      if (aValue != gCheckUpdateSecurityDefault)
        Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, aValue);
      else
        Services.prefs.clearUserPref(PREF_EM_CHECK_UPDATE_SECURITY);
    }
    return aValue;
  },

  get updateEnabled() {
    return gUpdateEnabled;
  },

  set updateEnabled(aValue) {
    aValue = !!aValue;
    if (aValue != gUpdateEnabled)
      Services.prefs.setBoolPref(PREF_EM_UPDATE_ENABLED, aValue);
    return aValue;
  },

  get hotfixID() {
    return gHotfixID;
  },
};

/**
 * Should not be used outside of core Mozilla code. This is a private API for
 * the startup and platform integration code to use. Refer to the methods on
 * AddonManagerInternal for documentation however note that these methods are
 * subject to change at any time.
 */
this.AddonManagerPrivate = {
  startup: function AMP_startup() {
    AddonManagerInternal.startup();
  },

  registerProvider: function AMP_registerProvider(aProvider, aTypes) {
    AddonManagerInternal.registerProvider(aProvider, aTypes);
  },

  unregisterProvider: function AMP_unregisterProvider(aProvider) {
    AddonManagerInternal.unregisterProvider(aProvider);
  },

  backgroundUpdateCheck: function AMP_backgroundUpdateCheck() {
    return AddonManagerInternal.backgroundUpdateCheck();
  },

  addStartupChange: function AMP_addStartupChange(aType, aID) {
    AddonManagerInternal.addStartupChange(aType, aID);
  },

  removeStartupChange: function AMP_removeStartupChange(aType, aID) {
    AddonManagerInternal.removeStartupChange(aType, aID);
  },

  notifyAddonChanged: function AMP_notifyAddonChanged(aID, aType, aPendingRestart) {
    AddonManagerInternal.notifyAddonChanged(aID, aType, aPendingRestart);
  },

  updateAddonAppDisabledStates: function AMP_updateAddonAppDisabledStates() {
    AddonManagerInternal.updateAddonAppDisabledStates();
  },

  updateAddonRepositoryData: function AMP_updateAddonRepositoryData(aCallback) {
    AddonManagerInternal.updateAddonRepositoryData(aCallback);
  },

  callInstallListeners: function AMP_callInstallListeners(...aArgs) {
    return AddonManagerInternal.callInstallListeners.apply(AddonManagerInternal,
                                                           aArgs);
  },

  callAddonListeners: function AMP_callAddonListeners(...aArgs) {
    AddonManagerInternal.callAddonListeners.apply(AddonManagerInternal, aArgs);
  },

  AddonAuthor: AddonAuthor,

  AddonScreenshot: AddonScreenshot,

  AddonCompatibilityOverride: AddonCompatibilityOverride,

  AddonType: AddonType,

  recordTimestamp: function AMP_recordTimestamp(name, value) {
    AddonManagerInternal.recordTimestamp(name, value);
  },

  _simpleMeasures: {},
  recordSimpleMeasure: function AMP_recordSimpleMeasure(name, value) {
    this._simpleMeasures[name] = value;
  },

  recordException: function AMP_recordException(aModule, aContext, aException) {
    let report = {
      module: aModule,
      context: aContext
    };

    if (typeof aException == "number") {
      report.message = Components.Exception("", aException).name;
    }
    else {
      report.message = aException.toString();
      if (aException.fileName) {
        report.file = aException.fileName;
        report.line = aException.lineNumber;
      }
    }

    this._simpleMeasures.exception = report;
  },

  getSimpleMeasures: function AMP_getSimpleMeasures() {
    return this._simpleMeasures;
  },

  getTelemetryDetails: function AMP_getTelemetryDetails() {
    return AddonManagerInternal.telemetryDetails;
  },

  setTelemetryDetails: function AMP_setTelemetryDetails(aProvider, aDetails) {
    AddonManagerInternal.telemetryDetails[aProvider] = aDetails;
  },

  // Start a timer, record a simple measure of the time interval when
  // timer.done() is called
  simpleTimer: function(aName) {
    let startTime = Date.now();
    return {
      done: () => this.recordSimpleMeasure(aName, Date.now() - startTime)
    };
  },

  /**
   * Helper to call update listeners when no update is available.
   *
   * This can be used as an implementation for Addon.findUpdates() when
   * no update mechanism is available.
   */
  callNoUpdateListeners: function (addon, listener, reason, appVersion, platformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in listener) {
      safeCall(listener.onNoCompatibilityUpdateAvailable.bind(listener), addon);
    }
    if ("onNoUpdateAvailable" in listener) {
      safeCall(listener.onNoUpdateAvailable.bind(listener), addon);
    }
    if ("onUpdateFinished" in listener) {
      safeCall(listener.onUpdateFinished.bind(listener), addon);
    }
  },
};

/**
 * This is the public API that UI and developers should be calling. All methods
 * just forward to AddonManagerInternal.
 */
this.AddonManager = {
  // Constants for the AddonInstall.state property
  // The install is available for download.
  STATE_AVAILABLE: 0,
  // The install is being downloaded.
  STATE_DOWNLOADING: 1,
  // The install is checking for compatibility information.
  STATE_CHECKING: 2,
  // The install is downloaded and ready to install.
  STATE_DOWNLOADED: 3,
  // The download failed.
  STATE_DOWNLOAD_FAILED: 4,
  // The add-on is being installed.
  STATE_INSTALLING: 5,
  // The add-on has been installed.
  STATE_INSTALLED: 6,
  // The install failed.
  STATE_INSTALL_FAILED: 7,
  // The install has been cancelled.
  STATE_CANCELLED: 8,

  // Constants representing different types of errors while downloading an
  // add-on.
  // The download failed due to network problems.
  ERROR_NETWORK_FAILURE: -1,
  // The downloaded file did not match the provided hash.
  ERROR_INCORRECT_HASH: -2,
  // The downloaded file seems to be corrupted in some way.
  ERROR_CORRUPT_FILE: -3,
  // An error occured trying to write to the filesystem.
  ERROR_FILE_ACCESS: -4,

  // These must be kept in sync with AddonUpdateChecker.
  // No error was encountered.
  UPDATE_STATUS_NO_ERROR: 0,
  // The update check timed out
  UPDATE_STATUS_TIMEOUT: -1,
  // There was an error while downloading the update information.
  UPDATE_STATUS_DOWNLOAD_ERROR: -2,
  // The update information was malformed in some way.
  UPDATE_STATUS_PARSE_ERROR: -3,
  // The update information was not in any known format.
  UPDATE_STATUS_UNKNOWN_FORMAT: -4,
  // The update information was not correctly signed or there was an SSL error.
  UPDATE_STATUS_SECURITY_ERROR: -5,
  // The update was cancelled.
  UPDATE_STATUS_CANCELLED: -6,

  // Constants to indicate why an update check is being performed
  // Update check has been requested by the user.
  UPDATE_WHEN_USER_REQUESTED: 1,
  // Update check is necessary to see if the Addon is compatibile with a new
  // version of the application.
  UPDATE_WHEN_NEW_APP_DETECTED: 2,
  // Update check is necessary because a new application has been installed.
  UPDATE_WHEN_NEW_APP_INSTALLED: 3,
  // Update check is a regular background update check.
  UPDATE_WHEN_PERIODIC_UPDATE: 16,
  // Update check is needed to check an Addon that is being installed.
  UPDATE_WHEN_ADDON_INSTALLED: 17,

  // Constants for operations in Addon.pendingOperations
  // Indicates that the Addon has no pending operations.
  PENDING_NONE: 0,
  // Indicates that the Addon will be enabled after the application restarts.
  PENDING_ENABLE: 1,
  // Indicates that the Addon will be disabled after the application restarts.
  PENDING_DISABLE: 2,
  // Indicates that the Addon will be uninstalled after the application restarts.
  PENDING_UNINSTALL: 4,
  // Indicates that the Addon will be installed after the application restarts.
  PENDING_INSTALL: 8,
  PENDING_UPGRADE: 16,

  // Constants for operations in Addon.operationsRequiringRestart
  // Indicates that restart isn't required for any operation.
  OP_NEEDS_RESTART_NONE: 0,
  // Indicates that restart is required for enabling the addon.
  OP_NEEDS_RESTART_ENABLE: 1,
  // Indicates that restart is required for disabling the addon.
  OP_NEEDS_RESTART_DISABLE: 2,
  // Indicates that restart is required for uninstalling the addon.
  OP_NEEDS_RESTART_UNINSTALL: 4,
  // Indicates that restart is required for installing the addon.
  OP_NEEDS_RESTART_INSTALL: 8,

  // Constants for permissions in Addon.permissions.
  // Indicates that the Addon can be uninstalled.
  PERM_CAN_UNINSTALL: 1,
  // Indicates that the Addon can be enabled by the user.
  PERM_CAN_ENABLE: 2,
  // Indicates that the Addon can be disabled by the user.
  PERM_CAN_DISABLE: 4,
  // Indicates that the Addon can be upgraded.
  PERM_CAN_UPGRADE: 8,
  // Indicates that the Addon can be set to be optionally enabled
  // on a case-by-case basis.
  PERM_CAN_ASK_TO_ACTIVATE: 16,

  // General descriptions of where items are installed.
  // Installed in this profile.
  SCOPE_PROFILE: 1,
  // Installed for all of this user's profiles.
  SCOPE_USER: 2,
  // Installed and owned by the application.
  SCOPE_APPLICATION: 4,
  // Installed for all users of the computer.
  SCOPE_SYSTEM: 8,
  // The combination of all scopes.
  SCOPE_ALL: 15,

  // 1-15 are different built-in views for the add-on type
  VIEW_TYPE_LIST: "list",

  TYPE_UI_HIDE_EMPTY: 16,
  // Indicates that this add-on type supports the ask-to-activate state.
  // That is, add-ons of this type can be set to be optionally enabled
  // on a case-by-case basis.
  TYPE_SUPPORTS_ASK_TO_ACTIVATE: 32,

  // Constants for Addon.applyBackgroundUpdates.
  // Indicates that the Addon should not update automatically.
  AUTOUPDATE_DISABLE: 0,
  // Indicates that the Addon should update automatically only if
  // that's the global default.
  AUTOUPDATE_DEFAULT: 1,
  // Indicates that the Addon should update automatically.
  AUTOUPDATE_ENABLE: 2,

  // Constants for how Addon options should be shown.
  // Options will be opened in a new window
  OPTIONS_TYPE_DIALOG: 1,
  // Options will be displayed within the AM detail view
  OPTIONS_TYPE_INLINE: 2,
  // Options will be displayed in a new tab, if possible
  OPTIONS_TYPE_TAB: 3,
  // Same as OPTIONS_TYPE_INLINE, but no Preferences button will be shown.
  // Used to indicate that only non-interactive information will be shown.
  OPTIONS_TYPE_INLINE_INFO: 4,

  // Constants for displayed or hidden options notifications
  // Options notification will be displayed
  OPTIONS_NOTIFICATION_DISPLAYED: "addon-options-displayed",
  // Options notification will be hidden
  OPTIONS_NOTIFICATION_HIDDEN: "addon-options-hidden",

  // Constants for getStartupChanges, addStartupChange and removeStartupChange
  // Add-ons that were detected as installed during startup. Doesn't include
  // add-ons that were pending installation the last time the application ran.
  STARTUP_CHANGE_INSTALLED: "installed",
  // Add-ons that were detected as changed during startup. This includes an
  // add-on moving to a different location, changing version or just having
  // been detected as possibly changed.
  STARTUP_CHANGE_CHANGED: "changed",
  // Add-ons that were detected as uninstalled during startup. Doesn't include
  // add-ons that were pending uninstallation the last time the application ran.
  STARTUP_CHANGE_UNINSTALLED: "uninstalled",
  // Add-ons that were detected as disabled during startup, normally because of
  // an application change making an add-on incompatible. Doesn't include
  // add-ons that were pending being disabled the last time the application ran.
  STARTUP_CHANGE_DISABLED: "disabled",
  // Add-ons that were detected as enabled during startup, normally because of
  // an application change making an add-on compatible. Doesn't include
  // add-ons that were pending being enabled the last time the application ran.
  STARTUP_CHANGE_ENABLED: "enabled",

  // Constants for the Addon.userDisabled property
  // Indicates that the userDisabled state of this add-on is currently
  // ask-to-activate. That is, it can be conditionally enabled on a
  // case-by-case basis.
  STATE_ASK_TO_ACTIVATE: "askToActivate",

#ifdef MOZ_EM_DEBUG
  get __AddonManagerInternal__() {
    return AddonManagerInternal;
  },
#endif

  getInstallForURL: function AM_getInstallForURL(aUrl, aCallback, aMimetype,
                                                 aHash, aName, aIcons,
                                                 aVersion, aLoadGroup) {
    AddonManagerInternal.getInstallForURL(aUrl, aCallback, aMimetype, aHash,
                                          aName, aIcons, aVersion, aLoadGroup);
  },

  getInstallForFile: function AM_getInstallForFile(aFile, aCallback, aMimetype) {
    AddonManagerInternal.getInstallForFile(aFile, aCallback, aMimetype);
  },

  /**
   * Gets an array of add-on IDs that changed during the most recent startup.
   *
   * @param  aType
   *         The type of startup change to get
   * @return An array of add-on IDs
   */
  getStartupChanges: function AM_getStartupChanges(aType) {
    if (!(aType in AddonManagerInternal.startupChanges))
      return [];
    return AddonManagerInternal.startupChanges[aType].slice(0);
  },

  getAddonByID: function AM_getAddonByID(aID, aCallback) {
    AddonManagerInternal.getAddonByID(aID, aCallback);
  },

  getAddonBySyncGUID: function AM_getAddonBySyncGUID(aGUID, aCallback) {
    AddonManagerInternal.getAddonBySyncGUID(aGUID, aCallback);
  },

  getAddonsByIDs: function AM_getAddonsByIDs(aIDs, aCallback) {
    AddonManagerInternal.getAddonsByIDs(aIDs, aCallback);
  },

  getAddonsWithOperationsByTypes:
  function AM_getAddonsWithOperationsByTypes(aTypes, aCallback) {
    AddonManagerInternal.getAddonsWithOperationsByTypes(aTypes, aCallback);
  },

  getAddonsByTypes: function AM_getAddonsByTypes(aTypes, aCallback) {
    AddonManagerInternal.getAddonsByTypes(aTypes, aCallback);
  },

  getAllAddons: function AM_getAllAddons(aCallback) {
    AddonManagerInternal.getAllAddons(aCallback);
  },

  getInstallsByTypes: function AM_getInstallsByTypes(aTypes, aCallback) {
    AddonManagerInternal.getInstallsByTypes(aTypes, aCallback);
  },

  getAllInstalls: function AM_getAllInstalls(aCallback) {
    AddonManagerInternal.getAllInstalls(aCallback);
  },

  mapURIToAddonID: function AM_mapURIToAddonID(aURI) {
    return AddonManagerInternal.mapURIToAddonID(aURI);
  },

  isInstallEnabled: function AM_isInstallEnabled(aType) {
    return AddonManagerInternal.isInstallEnabled(aType);
  },

  isInstallAllowed: function AM_isInstallAllowed(aType, aUri) {
    return AddonManagerInternal.isInstallAllowed(aType, aUri);
  },

  installAddonsFromWebpage: function AM_installAddonsFromWebpage(aType, aSource,
                                                                 aUri, aInstalls) {
    AddonManagerInternal.installAddonsFromWebpage(aType, aSource, aUri, aInstalls);
  },

  addManagerListener: function AM_addManagerListener(aListener) {
    AddonManagerInternal.addManagerListener(aListener);
  },

  removeManagerListener: function AM_removeManagerListener(aListener) {
    AddonManagerInternal.removeManagerListener(aListener);
  },

  addInstallListener: function AM_addInstallListener(aListener) {
    AddonManagerInternal.addInstallListener(aListener);
  },

  removeInstallListener: function AM_removeInstallListener(aListener) {
    AddonManagerInternal.removeInstallListener(aListener);
  },

  addAddonListener: function AM_addAddonListener(aListener) {
    AddonManagerInternal.addAddonListener(aListener);
  },

  removeAddonListener: function AM_removeAddonListener(aListener) {
    AddonManagerInternal.removeAddonListener(aListener);
  },

  addTypeListener: function AM_addTypeListener(aListener) {
    AddonManagerInternal.addTypeListener(aListener);
  },

  removeTypeListener: function AM_removeTypeListener(aListener) {
    AddonManagerInternal.removeTypeListener(aListener);
  },

  get addonTypes() {
    return AddonManagerInternal.addonTypes;
  },

  /**
   * Determines whether an Addon should auto-update or not.
   *
   * @param  aAddon
   *         The Addon representing the add-on
   * @return true if the addon should auto-update, false otherwise.
   */
  shouldAutoUpdate: function AM_shouldAutoUpdate(aAddon) {
    if (!aAddon || typeof aAddon != "object")
      throw Components.Exception("aAddon must be specified",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!("applyBackgroundUpdates" in aAddon))
      return false;
    if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_ENABLE)
      return true;
    if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_DISABLE)
      return false;
    return this.autoUpdateDefault;
  },

  get checkCompatibility() {
    return AddonManagerInternal.checkCompatibility;
  },

  set checkCompatibility(aValue) {
    AddonManagerInternal.checkCompatibility = aValue;
  },

  get strictCompatibility() {
    return AddonManagerInternal.strictCompatibility;
  },

  set strictCompatibility(aValue) {
    AddonManagerInternal.strictCompatibility = aValue;
  },

  get checkUpdateSecurityDefault() {
    return AddonManagerInternal.checkUpdateSecurityDefault;
  },

  get checkUpdateSecurity() {
    return AddonManagerInternal.checkUpdateSecurity;
  },

  set checkUpdateSecurity(aValue) {
    AddonManagerInternal.checkUpdateSecurity = aValue;
  },

  get updateEnabled() {
    return AddonManagerInternal.updateEnabled;
  },

  set updateEnabled(aValue) {
    AddonManagerInternal.updateEnabled = aValue;
  },

  get autoUpdateDefault() {
    return AddonManagerInternal.autoUpdateDefault;
  },

  set autoUpdateDefault(aValue) {
    AddonManagerInternal.autoUpdateDefault = aValue;
  },

  get hotfixID() {
    return AddonManagerInternal.hotfixID;
  },

  escapeAddonURI: function AM_escapeAddonURI(aAddon, aUri, aAppVersion) {
    return AddonManagerInternal.escapeAddonURI(aAddon, aUri, aAppVersion);
  }
};

// load the timestamps module into AddonManagerInternal
Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", AddonManagerInternal);
Object.freeze(AddonManagerInternal);
Object.freeze(AddonManagerPrivate);
Object.freeze(AddonManager);
