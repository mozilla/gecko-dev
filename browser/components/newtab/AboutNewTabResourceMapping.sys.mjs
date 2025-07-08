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
export const TRAINHOP_XPI_VERSION_PREF =
  "browser.newtabpage.trainhopAddon.version";

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
  trainhopAddonXPIVersion: {
    pref: TRAINHOP_XPI_VERSION_PREF,
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
  _addonIsXPI: null,
  _addonVersion: null,
  _addonListener: null,
  _builtinVersion: null,

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
    this.getBuiltinAddonVersion();
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
   * Retrieves the version of the built-in newtab add-on from AddonManager.
   * If AddonManager.getBuiltinAddonVersion hits an unexpected exception (e.g.
   * if the method is unexpectedly called before AddonManager and XPIProvider
   * are being started), it sets the _builtinVersion property to null and logs
   * a warning message.
   */
  getBuiltinAddonVersion() {
    try {
      this._builtinVersion =
        lazy.AddonManager.getBuiltinAddonVersion(BUILTIN_ADDON_ID);
    } catch (e) {
      this._builtinVersion = null;
      this.logger.warn(
        "Unexpected failure on retrieving builtin addon version",
        e
      );
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
    let isXPI = rootURI?.spec.endsWith(".xpi!/");

    // If we failed to retrieve the builtin add-on version, avoid mapping
    // XPI resources as an additional safety measure, because later it
    // wouldn't be possible to check if the builtin version is more recent
    // than the train-hop add-on version that may be already installed.
    if (isXPI && this._builtinVersion === null) {
      rootURI = null;
      isXPI = false;
    }

    // Do not use XPI resources to prepare to uninstall the train-hop add-on xpi
    // later in the current application session from updateTrainhopAddonState, if:
    //
    // - the train-hop add-on version set in the pref is empty (the client has been
    //   unenrolled in the previous browsing session and so we fallback to the
    //   resources bundled in the Desktop omni jar)
    // - the builtin add-on version is equal or greater than the train-hop add-on
    //   version (and so the application has been updated and the old train-hop
    //   add-on is obsolete and can be uninstalled).
    const shouldUninstallXPI = isXPI
      ? lazy.trainhopAddonXPIVersion === "" ||
        Services.vc.compare(this._builtinVersion, version) >= 0
      : false;

    if (!rootURI || inSafeMode || newTabAsAddonDisabled || shouldUninstallXPI) {
      const builtinAddonsURI = lazy.resProto.getSubstitution("builtin-addons");
      rootURI = Services.io.newURI("newtab/", null, builtinAddonsURI);
      version = null;
      isXPI = false;
    }
    return { isXPI, version, rootURI };
  },

  /**
   * Registers the resource://newtab and chrome://newtab resources, and also
   * kicks off dynamic Fluent and Glean registration if the add-on is installed
   * via an XPI.
   */
  registerNewTabResources() {
    const RES_PATH = "newtab";
    try {
      const { isXPI, version, rootURI } = this.getPreferredMapping();
      this._rootURISpec = rootURI.spec;
      this._addonVersion = version;
      this._addonIsXPI = isXPI;
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
    // That file will be generated by build script getting implemented with Bug 1960111
    const version = AppConstants.MOZ_APP_VERSION.match(/\d+/)[0];
    const metricsPath = `resource://newtab/webext-glue/metrics/runtime-metrics-${version}.json`;
    this.logger.debug(`Registering FOG Glean metrics from ${metricsPath}`);
    lazy.NewTabGleanUtils.registerMetricsAndPings(metricsPath);
  },

  /**
   * Updates the state of the train-hop add-on based on the Nimbus feature variables.
   *
   * @returns {Promise<void>}
   *   Resolves once the train-hop add-on has been staged to be installed or uninstalled (e.g.
   *   when the client has been unenrolled from the Nimbus feature), or after it has been
   *   determined that no action was needed (e.g. while running in safemode, or if the same
   *   or an higher add-on version than the train-hop add-on version is already in use,
   *   installed or pending to be installed). Rejects on failures or unexpected cancellations
   *   during installation or uninstallation process.
   */
  async updateTrainhopAddonState() {
    if (this.inSafeMode) {
      this.logger.debug(
        "train-hop add-on update state disabled while running in SafeMode"
      );
      return;
    }

    const nimbusFeature = lazy.NimbusFeatures[TRAINHOP_NIMBUS_FEATURE_ID];
    await nimbusFeature.ready();
    const { addon_version, xpi_download_path } = nimbusFeature.getAllVariables({
      defaultValues: { addon_version: null, xpi_download_path: null },
    });

    let addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);

    // Uninstall train-hop add-on xpi if its resources are not currently
    // being used and the client has been unenrolled from the newtabTrainhopAddon
    // Nimbus feature.
    if (!this._addonIsXPI && addon) {
      let changed = false;
      if (addon_version === null && xpi_download_path === null) {
        changed ||= await this.uninstallAddon({
          uninstallReason:
            "uninstalling train-hop add-on version on Nimbus feature unenrolled",
        });
        return;
      }

      if (
        this._builtinVersion &&
        Services.vc.compare(this._builtinVersion, addon.version) >= 0
      ) {
        changed ||= await this.uninstallAddon({
          uninstallReason:
            "uninstalling train-hop add-on version on builtin add-on with equal or higher version",
        });
      }

      // Retrieve the new add-on wrapper if the xpi version has been uninstalled.
      if (changed) {
        addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);
      }
    }

    // Record Nimbus feature newtabTrainhopAddon exposure event if NewTab
    // is currently using the resources from the train-hop add-on version.
    if (this._addonIsXPI && this._addonVersion === addon_version) {
      this.logger.debug(
        `train-hop add-on version ${addon_version} already in use`
      );
      // Record exposure event for the train hop feature if the train-hop
      // add-on version is already in use.
      nimbusFeature.recordExposureEvent({ once: true });
      return;
    }

    // Verify if the train-hop add-on version is already installed.
    if (addon?.version === addon_version) {
      this.logger.warn(
        `train-hop add-on version ${addon_version} already installed but not in use`
      );
      return;
    }

    if (!lazy.trainhopAddonXPIBaseURL) {
      this.logger.debug(
        "train-hop add-on download disabled on empty download base URL"
      );
      return;
    }

    if (addon_version == null) {
      this.logger.warn("train-hop failure: missing mandatory addon_version");
      return;
    }

    if (xpi_download_path == null) {
      this.logger.warn(
        "train-hop failure: missing mandatory xpi_download_path"
      );
      return;
    }

    const xpiDownloadURL = `${lazy.trainhopAddonXPIBaseURL}${xpi_download_path}`;
    await this._installTrainhopAddon({
      trainhopAddonVersion: addon_version,
      xpiDownloadURL,
    });
  },

  /**
   * Downloads and installs the newtab train-hop add-on version based on Nimbus feature configuration,
   * or record the Nimbus feature exposure event if the newtab train-hop add-on version is already in use.
   *
   * @param {object} params
   * @param {string} params.trainhopAddonVersion - The version of the train-hop add-on to install.
   * @param {string} params.xpiDownloadURL - The URL from which to download the XPI file.
   *
   * @returns {Promise<void>}
   *   Resolves when the train-hop add-on installation is completed or not needed, or rejects
   *   on failures or unexpected cancellations hit during the installation process.
   */
  async _installTrainhopAddon({ trainhopAddonVersion, xpiDownloadURL }) {
    if (
      this._builtinVersion &&
      Services.vc.compare(this._builtinVersion, trainhopAddonVersion) >= 0
    ) {
      this.logger.warn(
        `cancel xpi download on train-hop add-on version ${trainhopAddonVersion} on equal or higher builtin version ${this._builtinVersion}`
      );
      return;
    }

    let addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);
    if (
      addon?.version &&
      Services.vc.compare(addon.version, trainhopAddonVersion) >= 0
    ) {
      this.logger.warn(
        `cancel xpi download on train-hop add-on version ${trainhopAddonVersion} on equal or higher version ${addon.version} already installed`
      );
      return;
    }

    // Verify if there is already a pending install for the same or higher add-on version
    // (in case of multiple pending installations for the same add-on id, the last one wins).
    let pendingInstall = (await lazy.AddonManager.getAllInstalls())
      .filter(
        install =>
          install.addon?.id === BUILTIN_ADDON_ID &&
          install.state === lazy.AddonManager.STATE_POSTPONED
      )
      .pop();
    if (
      pendingInstall &&
      Services.vc.compare(pendingInstall.addon.version, trainhopAddonVersion) >=
        0
    ) {
      this.logger.debug(
        `cancel xpi download on train-hop add-on version ${trainhopAddonVersion} on equal or higher versions ${pendingInstall.addon.version} install already in progress`
      );
      return;
    }
    this.logger.log(
      `downloading train-hop add-on version ${trainhopAddonVersion} from ${xpiDownloadURL}`
    );
    try {
      let newInstall = await lazy.AddonManager.getInstallForURL(
        xpiDownloadURL,
        {
          telemetryInfo: { source: "nimbus:newtabTrainhopAddon" },
        }
      );
      const deferred = Promise.withResolvers();
      newInstall.addListener({
        onDownloadEnded() {
          if (
            newInstall.addon.id !== BUILTIN_ADDON_ID ||
            newInstall.addon.version !== trainhopAddonVersion
          ) {
            deferred.reject(
              new Error(
                `train-hop add-on install cancelled on mismatching add-on version` +
                  `(actual ${newInstall.addon.version}, expected ${trainhopAddonVersion})`
              )
            );
            newInstall.cancel();
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
      newInstall.install();
      await deferred.promise;
      this.logger.debug(
        `train-hop add-on ${trainhopAddonVersion} downloaded and pending install on next startup`
      );
    } catch (e) {
      this.logger.error(`train-hop add-on install failure: ${e}`);
    }
  },

  /**
   * Uninstalls the newtab add-on, if it exists and has the PERM_CAN_UNINSTALL permission,
   * optionally logs a reason for the add-on being uninstalled.
   *
   * @param {object} params
   * @param {string} [params.uninstallReason]
   *   Reason for uninstalling the add-on to log along with uninstalling
   *   the add-on.
   *
   * @returns {Promise<boolean>}
   *   Resolves once the add-on is uninstalled, if it was found and had the
   *   PERM_CAN_UNINSTALL permission, with a boolean set to true if the
   *   add-on was found and uninstalled.
   */
  async uninstallAddon({ uninstallReason } = {}) {
    let addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);
    if (addon && addon.permissions & lazy.AddonManager.PERM_CAN_UNINSTALL) {
      if (uninstallReason) {
        this.logger.info(uninstallReason);
      }
      await addon.uninstall();
      return true;
    }
    return false;
  },
};
