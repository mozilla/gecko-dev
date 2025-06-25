/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

let lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AboutHomeStartupCache: "resource:///modules/AboutHomeStartupCache.sys.mjs",
  NewTabGleanUtils: "resource://newtab/lib/NewTabGleanUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

const BUILTIN_ADDON_ID = "newtab@mozilla.org";
const BUNDLED_RESOURCES_URL = "resource://builtin-addons/newtab/";

/**
 * AboutNewTabResourceMapping is responsible for creating the mapping between
 * the built-in addon newtab code, and the chrome://newtab and resource://newtab
 * URI prefixes (which are also used by the component mode for newtab, and acts
 * as a compatibility layer).
 *
 * When the built-in addon newtab is being read in from an XPI, the
 * AboutNewTabResourceMapping is also responsible for doing dynamic Fluent
 * and Glean ping/metric registration.
 */
export var AboutNewTabResourceMapping = {
  initialized: false,
  log: null,

  _rootURISpec: null,
  _addonId: null,

  /**
   * This should be called early on in the lifetime of the browser, before any
   * attempt to load a resource from resource://newtab or chrome://newtab.
   *
   * This method is a no-op after the first call.
   */
  init() {
    if (this.initialized) {
      return;
    }

    this.logger = console.createInstance({
      prefix: "AboutNewTabResourceMapping",
      maxLogLevel: Services.prefs.getBoolPref(
        "browser.newtabpage.resource-mapping.log",
        false
      )
        ? "Debug"
        : "Warn",
    });
    this.logger.debug("Initializing");

    this.registerNewTabResources();

    // The newtab addon has a background.js script which defers updating until
    // the next restart. We still, however, want to blow away the about:home
    // startup cache when we notice this postponed install, to avoid loading
    // a cache created with another version of newtab.
    const addonInstallListener = {};
    addonInstallListener.onInstallPostponed = install => {
      if (install.addon.id === BUILTIN_ADDON_ID) {
        this.logger.debug(
          "Invalidating AboutHomeStartupCache on detected newly installed newtab resources"
        );
        lazy.AboutHomeStartupCache.clearCacheAndUninit();
      }
    };

    lazy.AddonManager.addInstallListener(addonInstallListener);
    this.initialized = true;
    this.logger.debug("Initialized");
  },

  /**
   * Registers the resource://newtab and chrome://newtab resources, and also
   * kicks off dynamic Fluent and Glean registration if the addon is installed
   * via an XPI.
   */
  registerNewTabResources() {
    const RES_PATH = "newtab";
    try {
      let policy = WebExtensionPolicy.getByID(BUILTIN_ADDON_ID);
      // Retrieve the mapping url (but fallback to the known url for the
      // newtab resources bundled in the Desktop omni jar if that fails).
      let { rootURI, version, id } = policy?.extension ?? {};
      if (!rootURI || Services.appinfo.inSafeMode) {
        rootURI = Services.io.newURI(BUNDLED_RESOURCES_URL);
        version = "bundled-resources-fallback";
      }
      this._rootURISpec = rootURI.spec;
      this._addonId = id;
      const isXPI = rootURI.spec.endsWith(".xpi!/");
      this.logger.log(
        `Mapping newtab resources from ${isXPI ? "XPI" : "built-in add-on"} version ${version} ` +
          `on application version ${AppConstants.MOZ_APP_VERSION_DISPLAY}`
      );
      lazy.resProto.setSubstitutionWithFlags(
        RES_PATH,
        rootURI,
        Ci.nsISubstitutingProtocolHandler.ALLOW_CONTENT_ACCESS
      );
      let aomStartup = Cc[
        "@mozilla.org/addons/addon-manager-startup;1"
      ].getService(Ci.amIAddonManagerStartup);
      const manifestURI = Services.io.newURI("manifest.json", null, rootURI);
      this._chromeHandle = aomStartup.registerChrome(manifestURI, [
        ["content", "newtab", "data/content", "contentaccessible=yes"],
      ]);

      if (isXPI) {
        // We must be a train-hopped XPI running in this app. This means we
        // may have Fluent files or Glean pings/metrics to register dynamically.
        this.registerFluentSources(rootURI);
        this.registerMetricsFromJson();
      }
      let redirector = Cc[
        "@mozilla.org/network/protocol/about;1?what=newtab"
      ].getService(Ci.nsIAboutModule).wrappedJSObject;
      redirector.notifyBuiltInAddonInitialized();
      Glean.newtab.addonReadySuccess.set(true);
      this.logger.debug("Newtab resource mapping completed successfully");
    } catch (e) {
      this.logger.error("Failed to complete resource mapping: ", e);
      Glean.newtab.addonReadySuccess.set(false);
      throw e;
    }
  },

  /**
   * Registers Fluent strings contained within the XPI.
   *
   * @param {nsIURI} rootURI
   *   The rootURI for the newtab addon.
   * @returns {Promise<undefined>}
   *   Resolves once the Fluent strings have been registered, or even if a
   *   failure to register them has occurred (which will log the error).
   */
  async registerFluentSources(rootURI) {
    try {
      const SUPPORTED_LOCALES = await fetch(
        rootURI.resolve("/locales/supported-locales.json")
      ).then(r => r.json());
      const newtabFileSource = new L10nFileSource(
        "newtab",
        "app",
        SUPPORTED_LOCALES,
        `resource://newtab/locales/{locale}/`
      );
      this._l10nFileSource = newtabFileSource;
      L10nRegistry.getInstance().registerSources([newtabFileSource]);
    } catch (e) {
      // TODO: consider if we should collect this in telemetry.
      this.logger.error(
        `Error on registering fluent files from ${rootURI.spec}:`,
        e
      );
    }
  },

  /**
   * Registers any dynamic Glean metrics that have been included with the XPI
   * version of the addon.
   */
  registerMetricsFromJson() {
    // The metrics we need to process were placed in webext-glue/metrics/runtime-metrics-<version>.json
    // That file will be generated by build scipt getting implemented with Bug 1960111
    const version = AppConstants.MOZ_APP_VERSION.match(/\d+/)[0];
    const metricsPath = `resource://newtab/webext-glue/metrics/runtime-metrics-${version}.json`;
    this.logger.debug(`Registering FOG Glean metrics from ${metricsPath}`);
    lazy.NewTabGleanUtils.registerMetricsAndPings(metricsPath);
  },

  /**
   * An external utility that should only be called in the event that we have
   * changed the configuration of the browser to use newtab as a built-in
   * component. This method will call into the AddonManager to uninstall the
   * remnants of the newtab addon, if they exist.
   *
   * @returns {Promise<undefined>}
   *   Resolves once the addon is uninstalled, if it was found.
   */
  async uninstallAddon() {
    let addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);
    if (addon) {
      await addon.uninstall();
    }
  },
};
