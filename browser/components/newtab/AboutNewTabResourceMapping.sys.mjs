/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

export const BUILTIN_ADDON_ID = "newtab@mozilla.org";
export const DISABLE_NEWTAB_AS_ADDON_PREF =
  "browser.newtabpage.disableNewTabAsAddon";
export const TRAINHOP_NIMBUS_FEATURE_ID = "newtabTrainhopAddon";
export const TRAINHOP_XPI_BASE_URL_PREF =
  "browser.newtabpage.trainhopAddon.xpiBaseURL";

const lazy = XPCOMUtils.declareLazy({
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AboutHomeStartupCache: "resource:///modules/AboutHomeStartupCache.sys.mjs",
  NewTabGleanUtils: "resource://newtab/lib/NewTabGleanUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",

  resProto: {
    service: "@mozilla.org/network/protocol;1?name=resource",
    iid: Ci.nsISubstitutingProtocolHandler,
  },
  aomStartup: {
    service: "@mozilla.org/addons/addon-manager-startup;1",
    iid: Ci.amIAddonManagerStartup,
  },
  aboutRedirector: {
    service: "@mozilla.org/network/protocol/about;1?what=newtab",
    iid: Ci.nsIAboutModule,
  },

  trainhopAddonXPIBaseURL: {
    pref: TRAINHOP_XPI_BASE_URL_PREF,
    default: "",
  },
});

/**
 * AboutNewTabResourceMapping is responsible for creating the mapping between
 * the built-in add-on newtab code, and the chrome://newtab and resource://newtab
 * URI prefixes (which are also used by the component mode for newtab, and acts
 * as a compatibility layer).
 *
 * When the built-in add-on newtab is being read in from an XPI, the
 * AboutNewTabResourceMapping is also responsible for doing dynamic Fluent
 * and Glean ping/metric registration.
 */
export var AboutNewTabResourceMapping = {
  initialized: false,
  log: null,
  newTabAsAddonDisabled: false,

  _rootURISpec: null,
  _addonVersion: null,
  _addonListener: null,

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

    // NOTE: this pref is read only once per session on purpose
    // (and it is expected to be used by the resource mapping logic
    // on the next application startup if flipped at runtime, e.g. as
    // part of an emergency pref flip through Nimbus).
    this.newTabAsAddonDisabled = Services.prefs.getBoolPref(
      DISABLE_NEWTAB_AS_ADDON_PREF,
      false
    );
    this.inSafeMode = Services.appinfo.inSafeMode;
    this.registerNewTabResources();
    this.addAddonListener();

    this.initialized = true;
    this.logger.debug("Initialized");
  },

  /**
   * Adds an add-on listener to detect postponed installations of the newtab add-on
   * and invalidate the AboutHomeStartupCache. This method is a no-op when the
   * emergency fallback `browser.newtabpage.disableNewTabAsAddon` about:config pref
   * is set to true.
   */
  addAddonListener() {
    if (!this._addonListener && !this.newTabAsAddonDisabled) {
      // The newtab add-on has a background.js script which defers updating until
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
      this._addonListener = addonInstallListener;
    }
  },

  /**
   * Gets the preferred mapping for newtab resources. This method tries to retrieve
   * the rootURI from the WebExtensionPolicy instance of the newtab add-on, or falling
   * back to the URI of the newtab resources bundled in the Desktop omni jar if not found.
   * The newtab resources bundled in the Desktop omni jar are instead always preferred
   * while running in safe mode or if the emergency fallback about:config pref
   * (`browser.newtabpage.disableNewTabAsAddon`) is set to true.
   *
   * @returns {{version: ?string, rootURI: nsIURI}}
   *   Returns the preferred newtab root URI for resource://newtab and chrome://newtab,
   *   along with add-on version if using the newtab add-on root URI, or null
   *   when the newtab add-on root URI was not selected as the preferred one.
   */
  getPreferredMapping() {
    const { inSafeMode, newTabAsAddonDisabled } = this;
    const policy = WebExtensionPolicy.getByID(BUILTIN_ADDON_ID);
    // Retrieve the mapping url (but fallback to the known url for the
    // newtab resources bundled in the Desktop omni jar if that fails).
    let { version, rootURI } = policy?.extension ?? {};
    if (!rootURI || inSafeMode || newTabAsAddonDisabled) {
      const builtinAddonsURI = lazy.resProto.getSubstitution("builtin-addons");
      rootURI = Services.io.newURI("newtab/", null, builtinAddonsURI);
      version = null;
    }
    return { version, rootURI };
  },

  /**
   * Registers the resource://newtab and chrome://newtab resources, and also
   * kicks off dynamic Fluent and Glean registration if the add-on is installed
   * via an XPI.
   */
  registerNewTabResources() {
    const RES_PATH = "newtab";
    try {
      const { version, rootURI } = this.getPreferredMapping();
      this._rootURISpec = rootURI.spec;
      this._addonVersion = version;
      const isXPI = rootURI.spec.endsWith(".xpi!/");
      this.logger.log(
        this.newTabAsAddonDisabled || !version
          ? `Mapping newtab resources from ${rootURI.spec}`
          : `Mapping newtab resources from ${isXPI ? "XPI" : "built-in add-on"} version ${version} ` +
              `on application version ${AppConstants.MOZ_APP_VERSION_DISPLAY}`
      );
      lazy.resProto.setSubstitutionWithFlags(
        RES_PATH,
        rootURI,
        Ci.nsISubstitutingProtocolHandler.ALLOW_CONTENT_ACCESS
      );
      const manifestURI = Services.io.newURI("manifest.json", null, rootURI);
      this._chromeHandle = lazy.aomStartup.registerChrome(manifestURI, [
        ["content", "newtab", "data/content", "contentaccessible=yes"],
      ]);

      if (isXPI) {
        // We must be a train-hopped XPI running in this app. This means we
        // may have Fluent files or Glean pings/metrics to register dynamically.
        this.registerFluentSources(rootURI);
        this.registerMetricsFromJson();
      }
      lazy.aboutRedirector.wrappedJSObject.notifyBuiltInAddonInitialized();
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
   *   The rootURI for the newtab add-on.
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
   * version of the add-on.
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
   * Downloads and installs the newtab train-hop add-on version based on Nimbus feature configuration,
   * or record the Nimbus feature exposure event if the newtab train-hop add-on version is already in use.
   *
   * @returns {Promise<void>}
   *   Resolves when the train-hop add-on installation is completed or not needed,
   *   or rejects on failures or unexpected cancellations hit during the installation
   *   process.
   */
  async installTrainhopAddon() {
    if (this.inSafeMode || !lazy.trainhopAddonXPIBaseURL) {
      this.logger.debug(
        this.inSafeMode
          ? `train-hop add-on download disabled while running in SafeMode`
          : `train-hop add-on download disabled on empty download base URL`
      );
      return;
    }
    const nimbusFeature = lazy.NimbusFeatures[TRAINHOP_NIMBUS_FEATURE_ID];
    await nimbusFeature.ready();
    const { addon_version, xpi_download_path } = nimbusFeature.getAllVariables({
      defaultValues: { addon_version: null, xpi_download_path: null },
    });
    if (!xpi_download_path) {
      this.logger.warn(
        `train-hop failure: missing mandatory xpi_download_path`
      );
      return;
    }
    if (!addon_version) {
      this.logger.warn(`train-hop failure: missing mandatory addon_version`);
      return;
    }
    let addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);
    if (addon?.version === addon_version) {
      if (this._addonVersion === addon.version) {
        this.logger.debug(
          `train-hop add-on version ${addon_version} already in use`
        );
        // Record exposure event for the train hop feature if the train-hop add-on version is
        // already in use.
        const isXPI = this._rootURISpec.endsWith(".xpi!/");
        if (isXPI) {
          nimbusFeature.recordExposureEvent({ once: true });
        }
      } else {
        this.logger.warn(
          `train-hop add-on version ${addon_version} already installed but not in use`
        );
      }
      return;
    } else if (
      addon?.version &&
      Services.vc.compare(addon.version, addon_version) > 0
    ) {
      this.logger.warn(
        `cancel xpi download on train-hop add-on version ${addon_version} lower than installed version ${addon.version}`
      );
      return;
    }
    // Download and install train-hop add-on from the url received through Nimbus.
    const xpiDownloadURL = `${lazy.trainhopAddonXPIBaseURL}${xpi_download_path}`;
    this.logger.log(
      `downloading train-hop add-on version ${addon_version} from ${xpiDownloadURL}`
    );
    try {
      let install = await lazy.AddonManager.getInstallForURL(xpiDownloadURL, {
        telemetryInfo: { source: "nimbus:newtabTrainhopAddon" },
      });
      const deferred = Promise.withResolvers();
      install.addListener({
        onDownloadEnded() {
          if (
            install.addon.id !== BUILTIN_ADDON_ID ||
            install.addon.version !== addon_version
          ) {
            deferred.reject(
              new Error(
                `train-hop add-on install cancelled on mismatching add-on version` +
                  `(actual ${install.addon.version}, expected ${addon_version})`
              )
            );
            install.cancel();
          }
        },
        onInstallPostponed() {
          deferred.resolve();
        },
        onDownloadCancelled() {
          deferred.reject(
            new Error(
              `Unexpected download cancelled while downloading xpi from ${xpiDownloadURL}`
            )
          );
        },
        onDownloadFailed() {
          deferred.reject(
            new Error(`Failed to download xpi from ${xpiDownloadURL}`)
          );
        },
        onInstallCancelled() {
          deferred.reject(
            new Error(
              `Unexpected install cancelled while installing xpi from ${xpiDownloadURL}`
            )
          );
        },
        onInstallFailed() {
          deferred.reject(
            new Error(`Failed to install xpi from ${xpiDownloadURL}`)
          );
        },
      });
      install.install();
      await deferred.promise;
      this.logger.debug(
        `train-hop add-on ${addon_version} downloaded and pending install on next startup`
      );
    } catch (e) {
      this.logger.error(`train-hop add-on install failure: ${e}`);
    }
  },

  /**
   * An external utility that should only be called in the event that we have
   * changed the configuration of the browser to use newtab as a built-in
   * component. This method will call into the AddonManager to uninstall the
   * remnants of the newtab add-on, if they exist.
   *
   * @returns {Promise<undefined>}
   *   Resolves once the add-on is uninstalled, if it was found.
   */
  async uninstallAddon() {
    let addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);
    if (addon) {
      await addon.uninstall();
    }
  },
};
