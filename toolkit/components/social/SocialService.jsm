/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["SocialService"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";
const ADDON_TYPE_SERVICE     = "service";
const ID_SUFFIX              = "@services.mozilla.org";
const STRING_TYPE_NAME       = "type.%ID%.name";

XPCOMUtils.defineLazyModuleGetter(this, "getFrameWorkerHandle", "resource://gre/modules/FrameWorker.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WorkerAPI", "resource://gre/modules/WorkerAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MozSocialAPI", "resource://gre/modules/MozSocialAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "closeAllChatWindows", "resource://gre/modules/MozSocialAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask", "resource://gre/modules/DeferredTask.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "etld",
                                   "@mozilla.org/network/effective-tld-service;1",
                                   "nsIEffectiveTLDService");

/**
 * The SocialService is the public API to social providers - it tracks which
 * providers are installed and enabled, and is the entry-point for access to
 * the provider itself.
 */

// Internal helper methods and state
let SocialServiceInternal = {
  _enabled: Services.prefs.getBoolPref("social.enabled"),
  get enabled() this._enabled,
  set enabled(val) {
    this._enabled = !!val;
    Services.prefs.setBoolPref("social.enabled", !!val);
  },

  get providerArray() {
    return [p for ([, p] of Iterator(this.providers))];
  },
  get manifests() {
    // Retrieve the manifests of installed providers from prefs
    let MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
    let prefs = MANIFEST_PREFS.getChildList("", []);
    for (let pref of prefs) {
      // we only consider manifests in user level prefs to be *installed*
      if (!MANIFEST_PREFS.prefHasUserValue(pref))
        continue;
      try {
        var manifest = JSON.parse(MANIFEST_PREFS.getComplexValue(pref, Ci.nsISupportsString).data);
        if (manifest && typeof(manifest) == "object" && manifest.origin)
          yield manifest;
      } catch (err) {
        Cu.reportError("SocialService: failed to load manifest: " + pref +
                       ", exception: " + err);
      }
    }
  },
  getManifestPrefname: function(origin) {
    // Retrieve the prefname for a given origin/manifest.
    // If no existing pref, return a generated prefname.
    let MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
    let prefs = MANIFEST_PREFS.getChildList("", []);
    for (let pref of prefs) {
      try {
        var manifest = JSON.parse(MANIFEST_PREFS.getComplexValue(pref, Ci.nsISupportsString).data);
        if (manifest.origin == origin) {
          return pref;
        }
      } catch (err) {
        Cu.reportError("SocialService: failed to load manifest: " + pref +
                       ", exception: " + err);
      }
    }
    let originUri = Services.io.newURI(origin, null, null);
    return originUri.hostPort.replace('.','-');
  },
  orderedProviders: function(aCallback) {
    if (SocialServiceInternal.providerArray.length < 2) {
      schedule(function () {
        aCallback(SocialServiceInternal.providerArray);
      });
      return;
    }
    // query moz_hosts for frecency.  since some providers may not have a
    // frecency entry, we need to later sort on our own. We use the providers
    // object below as an easy way to later record the frecency on the provider
    // object from the query results.
    let hosts = [];
    let providers = {};

    for (let p of SocialServiceInternal.providerArray) {
      p.frecency = 0;
      providers[p.domain] = p;
      hosts.push(p.domain);
    };

    // cannot bind an array to stmt.params so we have to build the string
    let stmt = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                 .DBConnection.createAsyncStatement(
      "SELECT host, frecency FROM moz_hosts WHERE host IN (" +
      [ '"' + host + '"' for each (host in hosts) ].join(",") + ") "
    );

    try {
      stmt.executeAsync({
        handleResult: function(aResultSet) {
          let row;
          while ((row = aResultSet.getNextRow())) {
            let rh = row.getResultByName("host");
            let frecency = row.getResultByName("frecency");
            providers[rh].frecency = parseInt(frecency) || 0;
          }
        },
        handleError: function(aError) {
          Cu.reportError(aError.message + " (Result = " + aError.result + ")");
        },
        handleCompletion: function(aReason) {
          // the query may not have returned all our providers, so we have
          // stamped the frecency on the provider and sort here. This makes sure
          // all enabled providers get sorted even with frecency zero.
          let providerList = SocialServiceInternal.providerArray;
          // reverse sort
          aCallback(providerList.sort(function(a, b) b.frecency - a.frecency));
        }
      });
    } finally {
      stmt.finalize();
    }
  }
};

XPCOMUtils.defineLazyGetter(SocialServiceInternal, "providers", function () {
  initService();
  let providers = {};
  for (let manifest of this.manifests) {
    try {
      if (ActiveProviders.has(manifest.origin)) {
        let provider = new SocialProvider(manifest);
        providers[provider.origin] = provider;
      }
    } catch (err) {
      Cu.reportError("SocialService: failed to load provider: " + manifest.origin +
                     ", exception: " + err);
    }
  }
  return providers;
});

function getOriginActivationType(origin) {
  let prefname = SocialServiceInternal.getManifestPrefname(origin);
  if (Services.prefs.getDefaultBranch("social.manifest.").getPrefType(prefname) == Services.prefs.PREF_STRING)
    return 'builtin';

  let whitelist = Services.prefs.getCharPref("social.whitelist").split(',');
  if (whitelist.indexOf(origin) >= 0)
    return 'whitelist';

  let directories = Services.prefs.getCharPref("social.directories").split(',');
  if (directories.indexOf(origin) >= 0)
    return 'directory';

  return 'foreign';
}

let ActiveProviders = {
  get _providers() {
    delete this._providers;
    this._providers = {};
    try {
      let pref = Services.prefs.getComplexValue("social.activeProviders",
                                                Ci.nsISupportsString);
      this._providers = JSON.parse(pref);
    } catch(ex) {}
    return this._providers;
  },

  has: function (origin) {
    return (origin in this._providers);
  },

  add: function (origin) {
    this._providers[origin] = 1;
    this._deferredTask.arm();
  },

  delete: function (origin) {
    delete this._providers[origin];
    this._deferredTask.arm();
  },

  flush: function () {
    this._deferredTask.disarm();
    this._persist();
  },

  get _deferredTask() {
    delete this._deferredTask;
    return this._deferredTask = new DeferredTask(this._persist.bind(this), 0);
  },

  _persist: function () {
    let string = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString);
    string.data = JSON.stringify(this._providers);
    Services.prefs.setComplexValue("social.activeProviders",
                                   Ci.nsISupportsString, string);
  }
};

function migrateSettings() {
  let activeProviders;
  try {
    activeProviders = Services.prefs.getCharPref("social.activeProviders");
  } catch(e) {
    // not set, we'll check if we need to migrate older prefs
  }
  if (activeProviders) {
    // migration from fx21 to fx22 or later
    // ensure any *builtin* provider in activeproviders is in user level prefs
    for (let origin in ActiveProviders._providers) {
      let prefname;
      let manifest;
      let defaultManifest;
      try {
        prefname = getPrefnameFromOrigin(origin);
        manifest = JSON.parse(Services.prefs.getComplexValue(prefname, Ci.nsISupportsString).data);
      } catch(e) {
        // Our preference is missing or bad, remove from ActiveProviders and
        // continue. This is primarily an error-case and should only be
        // reached by either messing with preferences or hitting the one or
        // two days of nightly that ran into it, so we'll flush right away.
        ActiveProviders.delete(origin);
        ActiveProviders.flush();
        continue;
      }
      let needsUpdate = !manifest.updateDate;
      // fx23 may have built-ins with shareURL
      try {
        defaultManifest = Services.prefs.getDefaultBranch(null)
                        .getComplexValue(prefname, Ci.nsISupportsString).data;
        defaultManifest = JSON.parse(defaultManifest);
        if (defaultManifest.shareURL && !manifest.shareURL) {
          manifest.shareURL = defaultManifest.shareURL;
          needsUpdate = true;
        }
      } catch(e) {
        // not a built-in, continue
      }
      if (needsUpdate) {
        // the provider was installed with an older build, so we will update the
        // timestamp and ensure the manifest is in user prefs
        delete manifest.builtin;
        // we're potentially updating for share, so always mark the updateDate
        manifest.updateDate = Date.now();
        if (!manifest.installDate)
          manifest.installDate = 0; // we don't know when it was installed

        let string = Cc["@mozilla.org/supports-string;1"].
                     createInstance(Ci.nsISupportsString);
        string.data = JSON.stringify(manifest);
        Services.prefs.setComplexValue(prefname, Ci.nsISupportsString, string);
      }
    }
    return;
  }

  // primary migration from pre-fx21
  let active;
  try {
    active = Services.prefs.getBoolPref("social.active");
  } catch(e) {}
  if (!active)
    return;

  // primary difference from SocialServiceInternal.manifests is that we
  // only read the default branch here.
  let manifestPrefs = Services.prefs.getDefaultBranch("social.manifest.");
  let prefs = manifestPrefs.getChildList("", []);
  for (let pref of prefs) {
    try {
      let manifest;
      try {
        manifest = JSON.parse(manifestPrefs.getComplexValue(pref, Ci.nsISupportsString).data);
      } catch(e) {
        // bad or missing preference, we wont update this one.
        continue;
      }
      if (manifest && typeof(manifest) == "object" && manifest.origin) {
        // our default manifests have been updated with the builtin flags as of
        // fx22, delete it so we can set the user-pref
        delete manifest.builtin;
        if (!manifest.updateDate) {
          manifest.updateDate = Date.now();
          manifest.installDate = 0; // we don't know when it was installed
        }

        let string = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
        string.data = JSON.stringify(manifest);
        // pref here is just the branch name, set the full pref name
        Services.prefs.setComplexValue("social.manifest." + pref, Ci.nsISupportsString, string);
        ActiveProviders.add(manifest.origin);
        ActiveProviders.flush();
        // social.active was used at a time that there was only one
        // builtin, we'll assume that is still the case
        return;
      }
    } catch (err) {
      Cu.reportError("SocialService: failed to load manifest: " + pref + ", exception: " + err);
    }
  }
}

function initService() {
  Services.obs.addObserver(function xpcomShutdown() {
    ActiveProviders.flush();
    SocialService._providerListeners = null;
    Services.obs.removeObserver(xpcomShutdown, "xpcom-shutdown");
  }, "xpcom-shutdown", false);

  try {
    migrateSettings();
  } catch(e) {
    // no matter what, if migration fails we do not want to render social
    // unusable. Worst case scenario is that, when upgrading Firefox, previously
    // enabled providers are not migrated.
    Cu.reportError("Error migrating social settings: " + e);
  }
  // Initialize the MozSocialAPI
  if (SocialServiceInternal.enabled)
    MozSocialAPI.enabled = true;
}

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}

// Public API
this.SocialService = {
  get hasEnabledProviders() {
    // used as an optimization during startup, can be used to check if further
    // initialization should be done (e.g. creating the instances of
    // SocialProvider and turning on UI). ActiveProviders may have changed and
    // not yet flushed so we check the active providers array
    for (let p in ActiveProviders._providers) {
      return true;
    };
    return false;
  },
  get enabled() {
    return SocialServiceInternal.enabled;
  },
  set enabled(val) {
    let enable = !!val;

    // Allow setting to the same value when in safe mode so the
    // feature can be force enabled.
    if (enable == SocialServiceInternal.enabled &&
        !Services.appinfo.inSafeMode)
      return;

    // if disabling, ensure all providers are actually disabled
    if (!enable)
      SocialServiceInternal.providerArray.forEach(function (p) p.enabled = false);

    SocialServiceInternal.enabled = enable;
    MozSocialAPI.enabled = enable;
    Services.telemetry.getHistogramById("SOCIAL_TOGGLED").add(enable);
  },

  // Adds and activates a builtin provider. The provider may or may not have
  // previously been added.  onDone is always called - with null if no such
  // provider exists, or the activated provider on success.
  addBuiltinProvider: function addBuiltinProvider(origin, onDone) {
    if (SocialServiceInternal.providers[origin]) {
      schedule(function() {
        onDone(SocialServiceInternal.providers[origin]);
      });
      return;
    }
    let manifest = SocialService.getManifestByOrigin(origin);
    if (manifest) {
      let addon = new AddonWrapper(manifest);
      AddonManagerPrivate.callAddonListeners("onEnabling", addon, false);
      addon.pendingOperations |= AddonManager.PENDING_ENABLE;
      this.addProvider(manifest, onDone);
      addon.pendingOperations -= AddonManager.PENDING_ENABLE;
      AddonManagerPrivate.callAddonListeners("onEnabled", addon);
      return;
    }
    schedule(function() {
      onDone(null);
    });
  },

  // Adds a provider given a manifest, and returns the added provider.
  addProvider: function addProvider(manifest, onDone) {
    if (SocialServiceInternal.providers[manifest.origin])
      throw new Error("SocialService.addProvider: provider with this origin already exists");

    let provider = new SocialProvider(manifest);
    SocialServiceInternal.providers[provider.origin] = provider;
    ActiveProviders.add(provider.origin);

    this.getOrderedProviderList(function (providers) {
      this._notifyProviderListeners("provider-enabled", provider.origin, providers);
      if (onDone)
        onDone(provider);
    }.bind(this));
  },

  // Removes a provider with the given origin, and notifies when the removal is
  // complete.
  removeProvider: function removeProvider(origin, onDone) {
    if (!(origin in SocialServiceInternal.providers))
      throw new Error("SocialService.removeProvider: no provider with origin " + origin + " exists!");

    let provider = SocialServiceInternal.providers[origin];
    let manifest = SocialService.getManifestByOrigin(origin);
    let addon = manifest && new AddonWrapper(manifest);
    if (addon) {
      AddonManagerPrivate.callAddonListeners("onDisabling", addon, false);
      addon.pendingOperations |= AddonManager.PENDING_DISABLE;
    }
    provider.enabled = false;

    ActiveProviders.delete(provider.origin);

    delete SocialServiceInternal.providers[origin];

    if (addon) {
      // we have to do this now so the addon manager ui will update an uninstall
      // correctly.
      addon.pendingOperations -= AddonManager.PENDING_DISABLE;
      AddonManagerPrivate.callAddonListeners("onDisabled", addon);
      AddonManagerPrivate.notifyAddonChanged(addon.id, ADDON_TYPE_SERVICE, false);
    }

    this.getOrderedProviderList(function (providers) {
      this._notifyProviderListeners("provider-disabled", origin, providers);
      if (onDone)
        onDone();
    }.bind(this));
  },

  // Returns a single provider object with the specified origin.  The provider
  // must be "installed" (ie, in ActiveProviders)
  getProvider: function getProvider(origin, onDone) {
    schedule((function () {
      onDone(SocialServiceInternal.providers[origin] || null);
    }).bind(this));
  },

  // Returns an unordered array of installed providers
  getProviderList: function(onDone) {
    schedule(function () {
      onDone(SocialServiceInternal.providerArray);
    });
  },

  getManifestByOrigin: function(origin) {
    for (let manifest of SocialServiceInternal.manifests) {
      if (origin == manifest.origin) {
        return manifest;
      }
    }
    return null;
  },

  // Returns an array of installed providers, sorted by frecency
  getOrderedProviderList: function(onDone) {
    SocialServiceInternal.orderedProviders(onDone);
  },

  getOriginActivationType: function (origin) {
    return getOriginActivationType(origin);
  },

  _providerListeners: new Map(),
  registerProviderListener: function registerProviderListener(listener) {
    this._providerListeners.set(listener, 1);
  },
  unregisterProviderListener: function unregisterProviderListener(listener) {
    this._providerListeners.delete(listener);
  },

  _notifyProviderListeners: function (topic, origin, providers) {
    for (let [listener, ] of this._providerListeners) {
      try {
        listener(topic, origin, providers);
      } catch (ex) {
        Components.utils.reportError("SocialService: provider listener threw an exception: " + ex);
      }
    }
  },

  _manifestFromData: function(type, data, principal) {
    let sameOriginRequired = ['workerURL', 'sidebarURL', 'shareURL', 'statusURL', 'markURL'];

    if (type == 'directory') {
      // directory provided manifests must have origin in manifest, use that
      if (!data['origin']) {
        Cu.reportError("SocialService.manifestFromData directory service provided manifest without origin.");
        return null;
      }
      let URI = Services.io.newURI(data.origin, null, null);
      principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(URI);
    }
    // force/fixup origin
    data.origin = principal.origin;

    // workerURL, sidebarURL is required and must be same-origin
    // iconURL and name are required
    // iconURL may be a different origin (CDN or data url support) if this is
    // a whitelisted or directory listed provider
    let providerHasFeatures = [url for (url of sameOriginRequired) if (data[url])].length > 0;
    if (!providerHasFeatures) {
      Cu.reportError("SocialService.manifestFromData manifest missing required urls.");
      return null;
    }
    if (!data['name'] || !data['iconURL']) {
      Cu.reportError("SocialService.manifestFromData manifest missing name or iconURL.");
      return null;
    }
    for (let url of sameOriginRequired) {
      if (data[url]) {
        try {
          data[url] = Services.io.newURI(principal.URI.resolve(data[url]), null, null).spec;
        } catch(e) {
          Cu.reportError("SocialService.manifestFromData same-origin missmatch in manifest for " + principal.origin);
          return null;
        }
      }
    }
    return data;
  },

  _getChromeWindow: function(aWindow) {
    var chromeWin = aWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow)
      .QueryInterface(Ci.nsIDOMChromeWindow);
    return chromeWin;
  },

  _showInstallNotification: function(aDOMDocument, aAddonInstaller) {
    let brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let requestingWindow = aDOMDocument.defaultView.top;
    let chromeWin = this._getChromeWindow(requestingWindow).wrappedJSObject;
    let browser = chromeWin.gBrowser.getBrowserForDocument(aDOMDocument);
    let requestingURI =  Services.io.newURI(aDOMDocument.location.href, null, null);

    let productName = brandBundle.GetStringFromName("brandShortName");

    let message = browserBundle.formatStringFromName("service.install.description",
                                                     [requestingURI.host, productName], 2);

    let action = {
      label: browserBundle.GetStringFromName("service.install.ok.label"),
      accessKey: browserBundle.GetStringFromName("service.install.ok.accesskey"),
      callback: function() {
        aAddonInstaller.install();
      },
    };

    let link = chromeWin.document.getElementById("servicesInstall-learnmore-link");
    link.value = browserBundle.GetStringFromName("service.install.learnmore");
    link.href = Services.urlFormatter.formatURLPref("app.support.baseURL") + "social-api";

    let anchor = "servicesInstall-notification-icon";
    let notificationid = "servicesInstall";
    chromeWin.PopupNotifications.show(browser, notificationid, message, anchor,
                                      action, [], {});
  },

  installProvider: function(aDOMDocument, data, installCallback) {
    let manifest;
    let installOrigin = aDOMDocument.nodePrincipal.origin;

    if (data) {
      let installType = getOriginActivationType(installOrigin);
      // if we get data, we MUST have a valid manifest generated from the data
      manifest = this._manifestFromData(installType, data, aDOMDocument.nodePrincipal);
      if (!manifest)
        throw new Error("SocialService.installProvider: service configuration is invalid from " + aDOMDocument.location.href);

      let addon = new AddonWrapper(manifest);
      if (addon && addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
        throw new Error("installProvider: provider with origin [" +
                        installOrigin + "] is blocklisted");
    }

    let id = getAddonIDFromOrigin(installOrigin);
    AddonManager.getAddonByID(id, function(aAddon) {
      if (aAddon && aAddon.userDisabled) {
        aAddon.cancelUninstall();
        aAddon.userDisabled = false;
      }
      schedule(function () {
        this._installProvider(aDOMDocument, manifest, aManifest => {
          this._notifyProviderListeners("provider-installed", aManifest.origin);
          installCallback(aManifest);
        });
      }.bind(this));
    }.bind(this));
  },

  _installProvider: function(aDOMDocument, manifest, installCallback) {
    let sourceURI = aDOMDocument.location.href;
    let installOrigin = aDOMDocument.nodePrincipal.origin;

    let installType = getOriginActivationType(installOrigin);
    let installer;
    switch(installType) {
      case "foreign":
        if (!Services.prefs.getBoolPref("social.remote-install.enabled"))
          throw new Error("Remote install of services is disabled");
        if (!manifest)
          throw new Error("Cannot install provider without manifest data");

        installer = new AddonInstaller(sourceURI, manifest, installCallback);
        this._showInstallNotification(aDOMDocument, installer);
        break;
      case "builtin":
        // for builtin, we already have a manifest, but it can be overridden
        // we need to return the manifest in the installcallback, so fetch
        // it if we have it.  If there is no manifest data for the builtin,
        // the install request MUST be from the provider, otherwise we have
        // no way to know what provider we're trying to enable.  This is
        // primarily an issue for "version zero" providers that did not
        // send the manifest with the dom event for activation.
        if (!manifest) {
          let prefname = getPrefnameFromOrigin(installOrigin);
          manifest = Services.prefs.getDefaultBranch(null)
                          .getComplexValue(prefname, Ci.nsISupportsString).data;
          manifest = JSON.parse(manifest);
          // ensure we override a builtin manifest by having a different value in it
          if (manifest.builtin)
            delete manifest.builtin;
        }
      case "directory":
        // a manifest is requried, and will have been vetted by reviewers
      case "whitelist":
        // a manifest is required, we'll catch a missing manifest below.
        if (!manifest)
          throw new Error("Cannot install provider without manifest data");
        installer = new AddonInstaller(sourceURI, manifest, installCallback);
        this._showInstallNotification(aDOMDocument, installer);
        break;
      default:
        throw new Error("SocialService.installProvider: Invalid install type "+installType+"\n");
        break;
    }
  },

  createWrapper: function(manifest) {
    return new AddonWrapper(manifest);
  },

  /**
   * updateProvider is used from the worker to self-update.  Since we do not
   * have knowledge of the currently selected provider here, we will notify
   * the front end to deal with any reload.
   */
  updateProvider: function(aUpdateOrigin, aManifest) {
    let originUri = Services.io.newURI(aUpdateOrigin, null, null);
    let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(originUri);
    let installType = this.getOriginActivationType(aUpdateOrigin);
    // if we get data, we MUST have a valid manifest generated from the data
    let manifest = this._manifestFromData(installType, aManifest, principal);
    if (!manifest)
      throw new Error("SocialService.installProvider: service configuration is invalid from " + aUpdateOrigin);

    // overwrite the preference
    let string = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString);
    string.data = JSON.stringify(manifest);
    Services.prefs.setComplexValue(getPrefnameFromOrigin(manifest.origin), Ci.nsISupportsString, string);

    // overwrite the existing provider then notify the front end so it can
    // handle any reload that might be necessary.
    if (ActiveProviders.has(manifest.origin)) {
      let provider = new SocialProvider(manifest);
      SocialServiceInternal.providers[provider.origin] = provider;
      // update the cache and ui, reload provider if necessary
      this.getOrderedProviderList(providers => {
        this._notifyProviderListeners("provider-update", provider.origin, providers);
      });
    }

  },

  uninstallProvider: function(origin, aCallback) {
    let manifest = SocialService.getManifestByOrigin(origin);
    let addon = new AddonWrapper(manifest);
    addon.uninstall(aCallback);
  }
};

/**
 * The SocialProvider object represents a social provider, and allows
 * access to its FrameWorker (if it has one).
 *
 * @constructor
 * @param {jsobj} object representing the manifest file describing this provider
 * @param {bool} boolean indicating whether this provider is "built in"
 */
function SocialProvider(input) {
  if (!input.name)
    throw new Error("SocialProvider must be passed a name");
  if (!input.origin)
    throw new Error("SocialProvider must be passed an origin");

  let addon = new AddonWrapper(input);
  if (addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
    throw new Error("SocialProvider: provider with origin [" +
                    input.origin + "] is blocklisted");

  this.name = input.name;
  this.iconURL = input.iconURL;
  this.icon32URL = input.icon32URL;
  this.icon64URL = input.icon64URL;
  this.workerURL = input.workerURL;
  this.sidebarURL = input.sidebarURL;
  this.shareURL = input.shareURL;
  this.statusURL = input.statusURL;
  this.markURL = input.markURL;
  this.markedIcon = input.markedIcon;
  this.unmarkedIcon = input.unmarkedIcon;
  this.origin = input.origin;
  let originUri = Services.io.newURI(input.origin, null, null);
  this.principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(originUri);
  this.ambientNotificationIcons = {};
  this.errorState = null;
  this.frecency = 0;

  let activationType = getOriginActivationType(input.origin);
  this.blessed = activationType == "builtin" ||
                 activationType == "whitelist";

  try {
    this.domain = etld.getBaseDomainFromHost(originUri.host);
  } catch(e) {
    this.domain = originUri.host;
  }
}

SocialProvider.prototype = {
  reload: function() {
    this._terminate();
    this._activate();
    Services.obs.notifyObservers(null, "social:provider-reload", this.origin);
  },

  // Provider enabled/disabled state. Disabled providers do not have active
  // connections to their FrameWorkers.
  _enabled: false,
  get enabled() {
    return this._enabled;
  },
  set enabled(val) {
    let enable = !!val;
    if (enable == this._enabled)
      return;

    this._enabled = enable;

    if (enable) {
      this._activate();
    } else {
      this._terminate();
    }
  },

  get manifest() {
    return SocialService.getManifestByOrigin(this.origin);
  },

  // Reference to a workerAPI object for this provider. Null if the provider has
  // no FrameWorker, or is disabled.
  workerAPI: null,

  // Contains information related to the user's profile. Populated by the
  // workerAPI via updateUserProfile.
  // Properties:
  //   iconURL, portrait, userName, displayName, profileURL
  // See https://github.com/mozilla/socialapi-dev/blob/develop/docs/socialAPI.md
  // A value of null or an empty object means 'user not logged in'.
  // A value of undefined means the service has not yet told us the status of
  // the profile (ie, the service is still loading/initing, or the provider has
  // no FrameWorker)
  // This distinction might be used to cache certain data between runs - eg,
  // browser-social.js caches the notification icons so they can be displayed
  // quickly at startup without waiting for the provider to initialize -
  // 'undefined' means 'ok to use cached values' versus 'null' meaning 'cached
  // values aren't to be used as the user is logged out'.
  profile: undefined,

  // Map of objects describing the provider's notification icons, whose
  // properties include:
  //   name, iconURL, counter, contentPanel
  // See https://developer.mozilla.org/en-US/docs/Social_API
  ambientNotificationIcons: null,

  // Called by the workerAPI to update our profile information.
  updateUserProfile: function(profile) {
    if (!profile)
      profile = {};
    let accountChanged = !this.profile || this.profile.userName != profile.userName;
    this.profile = profile;

    // Sanitize the portrait from any potential script-injection.
    if (profile.portrait) {
      try {
        let portraitUri = Services.io.newURI(profile.portrait, null, null);

        let scheme = portraitUri ? portraitUri.scheme : "";
        if (scheme != "data" && scheme != "http" && scheme != "https") {
          profile.portrait = "";
        }
      } catch (ex) {
        profile.portrait = "";
      }
    }

    if (profile.iconURL)
      this.iconURL = profile.iconURL;

    if (!profile.displayName)
      profile.displayName = profile.userName;

    // if no userName, consider this a logged out state, emtpy the
    // users ambient notifications.  notify both profile and ambient
    // changes to clear everything
    if (!profile.userName) {
      this.profile = {};
      this.ambientNotificationIcons = {};
      Services.obs.notifyObservers(null, "social:ambient-notification-changed", this.origin);
    }

    Services.obs.notifyObservers(null, "social:profile-changed", this.origin);
    if (accountChanged)
      closeAllChatWindows(this);
  },

  haveLoggedInUser: function () {
    return !!(this.profile && this.profile.userName);
  },

  // Called by the workerAPI to add/update a notification icon.
  setAmbientNotification: function(notification) {
    if (!this.profile.userName)
      throw new Error("unable to set notifications while logged out");
    if (!this.ambientNotificationIcons[notification.name] &&
        Object.keys(this.ambientNotificationIcons).length >= 3) {
      throw new Error("ambient notification limit reached");
    }
    this.ambientNotificationIcons[notification.name] = notification;

    Services.obs.notifyObservers(null, "social:ambient-notification-changed", this.origin);
  },

  // Internal helper methods
  _activate: function _activate() {
    // Initialize the workerAPI and its port first, so that its initialization
    // occurs before any other messages are processed by other ports.
    let workerAPIPort = this.getWorkerPort();
    if (workerAPIPort)
      this.workerAPI = new WorkerAPI(this, workerAPIPort);
  },

  _terminate: function _terminate() {
    closeAllChatWindows(this);
    if (this.workerURL) {
      try {
        getFrameWorkerHandle(this.workerURL).terminate();
      } catch (e) {
        Cu.reportError("SocialProvider FrameWorker termination failed: " + e);
      }
    }
    if (this.workerAPI) {
      this.workerAPI.terminate();
    }
    this.errorState = null;
    this.workerAPI = null;
    this.profile = undefined;
  },

  /**
   * Instantiates a FrameWorker for the provider if one doesn't exist, and
   * returns a reference to a new port to that FrameWorker.
   *
   * Returns null if this provider has no workerURL, or is disabled.
   *
   * @param {DOMWindow} window (optional)
   */
  getWorkerPort: function getWorkerPort(window) {
    if (!this.workerURL || !this.enabled)
      return null;
    // Only allow localStorage in the frameworker for blessed providers
    let allowLocalStorage = this.blessed;
    let handle = getFrameWorkerHandle(this.workerURL, window,
                                      "SocialProvider:" + this.origin, this.origin,
                                      allowLocalStorage);
    return handle.port;
  },

  /**
   * Checks if a given URI is of the same origin as the provider.
   *
   * Returns true or false.
   *
   * @param {URI or string} uri
   */
  isSameOrigin: function isSameOrigin(uri, allowIfInheritsPrincipal) {
    if (!uri)
      return false;
    if (typeof uri == "string") {
      try {
        uri = Services.io.newURI(uri, null, null);
      } catch (ex) {
        // an invalid URL can't be loaded!
        return false;
      }
    }
    try {
      this.principal.checkMayLoad(
        uri, // the thing to check.
        false, // reportError - we do our own reporting when necessary.
        allowIfInheritsPrincipal
      );
      return true;
    } catch (ex) {
      return false;
    }
  },

  /**
   * Resolve partial URLs for a provider.
   *
   * Returns nsIURI object or null on failure
   *
   * @param {string} url
   */
  resolveUri: function resolveUri(url) {
    try {
      let fullURL = this.principal.URI.resolve(url);
      return Services.io.newURI(fullURL, null, null);
    } catch (ex) {
      Cu.reportError("mozSocial: failed to resolve window URL: " + url + "; " + ex);
      return null;
    }
  }
}

function getAddonIDFromOrigin(origin) {
  let originUri = Services.io.newURI(origin, null, null);
  return originUri.host + ID_SUFFIX;
}

function getPrefnameFromOrigin(origin) {
  return "social.manifest." + SocialServiceInternal.getManifestPrefname(origin);
}

function AddonInstaller(sourceURI, aManifest, installCallback) {
  aManifest.updateDate = Date.now();
  // get the existing manifest for installDate
  let manifest = SocialService.getManifestByOrigin(aManifest.origin);
  let isNewInstall = !manifest;
  if (manifest && manifest.installDate)
    aManifest.installDate = manifest.installDate;
  else
    aManifest.installDate = aManifest.updateDate;

  this.sourceURI = sourceURI;
  this.install = function() {
    let addon = this.addon;
    if (isNewInstall) {
      AddonManagerPrivate.callInstallListeners("onExternalInstall", null, addon, null, false);
      AddonManagerPrivate.callAddonListeners("onInstalling", addon, false);
    }

    let string = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString);
    string.data = JSON.stringify(aManifest);
    Services.prefs.setComplexValue(getPrefnameFromOrigin(aManifest.origin), Ci.nsISupportsString, string);

    if (isNewInstall) {
      AddonManagerPrivate.callAddonListeners("onInstalled", addon);
    }
    installCallback(aManifest);
  };
  this.cancel = function() {
    Services.prefs.clearUserPref(getPrefnameFromOrigin(aManifest.origin))
  },
  this.addon = new AddonWrapper(aManifest);
};

var SocialAddonProvider = {
  startup: function() {},

  shutdown: function() {},

  updateAddonAppDisabledStates: function() {
    // we wont bother with "enabling" services that are released from blocklist
    for (let manifest of SocialServiceInternal.manifests) {
      try {
        if (ActiveProviders.has(manifest.origin)) {
          let addon = new AddonWrapper(manifest);
          if (addon.blocklistState != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
            SocialService.removeProvider(manifest.origin);
          }
        }
      } catch(e) {
        Cu.reportError(e);
      }
    }
  },

  getAddonByID: function(aId, aCallback) {
    for (let manifest of SocialServiceInternal.manifests) {
      if (aId == getAddonIDFromOrigin(manifest.origin)) {
        aCallback(new AddonWrapper(manifest));
        return;
      }
    }
    aCallback(null);
  },

  getAddonsByTypes: function(aTypes, aCallback) {
    if (aTypes && aTypes.indexOf(ADDON_TYPE_SERVICE) == -1) {
      aCallback([]);
      return;
    }
    aCallback([new AddonWrapper(a) for each (a in SocialServiceInternal.manifests)]);
  },

  removeAddon: function(aAddon, aCallback) {
    AddonManagerPrivate.callAddonListeners("onUninstalling", aAddon, false);
    aAddon.pendingOperations |= AddonManager.PENDING_UNINSTALL;
    Services.prefs.clearUserPref(getPrefnameFromOrigin(aAddon.manifest.origin));
    aAddon.pendingOperations -= AddonManager.PENDING_UNINSTALL;
    AddonManagerPrivate.callAddonListeners("onUninstalled", aAddon);
    SocialService._notifyProviderListeners("provider-uninstalled", aAddon.manifest.origin);
    if (aCallback)
      schedule(aCallback);
  }
}


function AddonWrapper(aManifest) {
  this.manifest = aManifest;
  this.id = getAddonIDFromOrigin(this.manifest.origin);
  this._pending = AddonManager.PENDING_NONE;
}
AddonWrapper.prototype = {
  get type() {
    return ADDON_TYPE_SERVICE;
  },

  get appDisabled() {
    return this.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED;
  },

  set softDisabled(val) {
    this.userDisabled = val;
  },

  get softDisabled() {
    return this.userDisabled;
  },

  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get scope() {
    return AddonManager.SCOPE_PROFILE;
  },

  get foreignInstall() {
    return false;
  },

  isCompatibleWith: function(appVersion, platformVersion) {
    return true;
  },

  get providesUpdatesSecurely() {
    return true;
  },

  get blocklistState() {
    return Services.blocklist.getAddonBlocklistState(this);
  },

  get blocklistURL() {
    return Services.blocklist.getAddonBlocklistURL(this);
  },

  get screenshots() {
    return [];
  },

  get pendingOperations() {
    return this._pending || AddonManager.PENDING_NONE;
  },
  set pendingOperations(val) {
    this._pending = val;
  },

  get operationsRequiringRestart() {
    return AddonManager.OP_NEEDS_RESTART_NONE;
  },

  get size() {
    return null;
  },

  get permissions() {
    let permissions = 0;
    // any "user defined" manifest can be removed
    if (Services.prefs.prefHasUserValue(getPrefnameFromOrigin(this.manifest.origin)))
      permissions = AddonManager.PERM_CAN_UNINSTALL;
    if (!this.appDisabled) {
      if (this.userDisabled) {
        permissions |= AddonManager.PERM_CAN_ENABLE;
      } else {
        permissions |= AddonManager.PERM_CAN_DISABLE;
      }
    }
    return permissions;
  },

  findUpdates: function(listener, reason, appVersion, platformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in listener)
      listener.onNoCompatibilityUpdateAvailable(this);
    if ("onNoUpdateAvailable" in listener)
      listener.onNoUpdateAvailable(this);
    if ("onUpdateFinished" in listener)
      listener.onUpdateFinished(this);
  },

  get isActive() {
    return ActiveProviders.has(this.manifest.origin);
  },

  get name() {
    return this.manifest.name;
  },
  get version() {
    return this.manifest.version ? this.manifest.version : "";
  },

  get iconURL() {
    return this.manifest.icon32URL ? this.manifest.icon32URL : this.manifest.iconURL;
  },
  get icon64URL() {
    return this.manifest.icon64URL;
  },
  get icons() {
    let icons = {
      16: this.manifest.iconURL
    };
    if (this.manifest.icon32URL)
      icons[32] = this.manifest.icon32URL;
    if (this.manifest.icon64URL)
      icons[64] = this.manifest.icon64URL;
    return icons;
  },

  get description() {
    return this.manifest.description;
  },
  get homepageURL() {
    return this.manifest.homepageURL;
  },
  get defaultLocale() {
    return this.manifest.defaultLocale;
  },
  get selectedLocale() {
    return this.manifest.selectedLocale;
  },

  get installDate() {
    return this.manifest.installDate ? new Date(this.manifest.installDate) : null;
  },
  get updateDate() {
    return this.manifest.updateDate ? new Date(this.manifest.updateDate) : null;
  },

  get creator() {
    return new AddonManagerPrivate.AddonAuthor(this.manifest.author);
  },

  get userDisabled() {
    return this.appDisabled || !ActiveProviders.has(this.manifest.origin);
  },

  set userDisabled(val) {
    if (val == this.userDisabled)
      return val;
    if (val) {
      SocialService.removeProvider(this.manifest.origin);
    } else if (!this.appDisabled) {
      SocialService.addBuiltinProvider(this.manifest.origin);
    }
    return val;
  },

  uninstall: function(aCallback) {
    let prefName = getPrefnameFromOrigin(this.manifest.origin);
    if (Services.prefs.prefHasUserValue(prefName)) {
      if (ActiveProviders.has(this.manifest.origin)) {
        SocialService.removeProvider(this.manifest.origin, function() {
          SocialAddonProvider.removeAddon(this, aCallback);
        }.bind(this));
      } else {
        SocialAddonProvider.removeAddon(this, aCallback);
      }
    } else {
      schedule(aCallback);
    }
  },

  cancelUninstall: function() {
    this._pending -= AddonManager.PENDING_UNINSTALL;
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", this);
  }
};


AddonManagerPrivate.registerProvider(SocialAddonProvider, [
  new AddonManagerPrivate.AddonType(ADDON_TYPE_SERVICE, URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 10000)
]);
