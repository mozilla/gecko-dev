/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Represents a WebExtension add-on in the parent process. This gives some metadata about
 * the add-on and watches for uninstall events. This uses a proxy to access the
 * WebExtension in the WebExtension process via the message manager.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  webExtensionDescriptorSpec,
} = require("resource://devtools/shared/specs/descriptors/webextension.js");

const {
  createWebExtensionSessionContext,
} = require("resource://devtools/server/actors/watcher/session-context.js");

const lazy = {};
loader.lazyGetter(lazy, "AddonManager", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
    { global: "shared" }
  ).AddonManager;
});
loader.lazyGetter(lazy, "ExtensionParent", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
    { global: "shared" }
  ).ExtensionParent;
});
loader.lazyRequireGetter(
  this,
  "WatcherActor",
  "resource://devtools/server/actors/watcher.js",
  true
);

const { WEBEXTENSION_FALLBACK_DOC_URL } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/browsing-context-helpers.sys.mjs",
  { global: "contextual" }
);

const BGSCRIPT_STATUSES = {
  RUNNING: "RUNNING",
  STOPPED: "STOPPED",
};

/**
 * Creates the actor that represents the addon in the parent process, which relies
 * on its child Watcher Actor to expose all WindowGlobal target actors for all
 * the active documents involved in the debugged addon.
 *
 * The WebExtensionDescriptorActor subscribes itself as an AddonListener on the AddonManager
 * and forwards this events to child actor (e.g. on addon reload or when the addon is
 * uninstalled completely) and connects to the child extension process using a `browser`
 * element provided by the extension internals (it is not related to any single extension,
 * but it will be created automatically to the currently selected "WebExtensions OOP mode"
 * and it persist across the extension reloads.
 *
 * The descriptor will also be persisted when the target actor is destroyed, so
 * that we can reuse the same descriptor for several remote debugging toolboxes
 * from about:debugging.
 *
 * WebExtensionDescriptorActor is a child of RootActor, it can be retrieved via
 * RootActor.listAddons request.
 *
 * @param {DevToolsServerConnection} conn
 *        The connection to the client.
 * @param {AddonWrapper} addon
 *        The target addon.
 */
class WebExtensionDescriptorActor extends Actor {
  constructor(conn, addon) {
    super(conn, webExtensionDescriptorSpec);
    this.addon = addon;
    this.addonId = addon.id;

    this.destroy = this.destroy.bind(this);

    lazy.AddonManager.addAddonListener(this);
  }

  form() {
    const { addonId } = this;
    const policy = lazy.ExtensionParent.WebExtensionPolicy.getByID(addonId);
    const persistentBackgroundScript =
      lazy.ExtensionParent.DebugUtils.hasPersistentBackgroundScript(addonId);
    const backgroundScriptStatus = this._getBackgroundScriptStatus();

    return {
      actor: this.actorID,
      backgroundScriptStatus,
      // Note that until the policy becomes active,
      // getWatcher will fail attaching to the web extension:
      // https://searchfox.org/mozilla-central/rev/526a5089c61db85d4d43eb0e46edaf1f632e853a/toolkit/components/extensions/WebExtensionPolicy.cpp#551-553
      debuggable: policy?.active && this.addon.isDebuggable,
      hidden: this.addon.hidden,
      // iconDataURL is available after calling loadIconDataURL
      iconDataURL: this._iconDataURL,
      iconURL: this.addon.iconURL,
      id: addonId,
      isSystem: this.addon.isSystem,
      isWebExtension: this.addon.isWebExtension,
      manifestURL: policy && policy.getURL("manifest.json"),
      name: this.addon.name,
      persistentBackgroundScript,
      temporarilyInstalled: this.addon.temporarilyInstalled,
      traits: {
        supportsReloadDescriptor: true,
        // Supports the Watcher actor. Can be removed as part of Bug 1680280.
        watcher: true,
        // @backward-compat { version 133 } Firefox 133 started supporting server targets by default.
        // Once this is the only supported version, we can remove the traits and consider it always true in the frontend.
        isServerTargetSwitchingEnabled: true,
      },
      url: this.addon.sourceURI ? this.addon.sourceURI.spec : undefined,
      warnings: lazy.ExtensionParent.DebugUtils.getExtensionManifestWarnings(
        this.addonId
      ),
    };
  }

  /**
   * Return a Watcher actor, allowing to keep track of targets which
   * already exists or will be created. It also helps knowing when they
   * are destroyed.
   */
  async getWatcher(config = {}) {
    if (!this.watcher) {
      // Spawn an empty document so that we always have an active WindowGlobal,
      // so that we can always instantiate a top level WindowGlobal target to the frontend.
      await this.#createFallbackDocument();

      this.watcher = new WatcherActor(
        this.conn,
        createWebExtensionSessionContext(
          {
            addonId: this.addonId,
          },
          config
        )
      );
      this.manage(this.watcher);
    }
    return this.watcher;
  }

  /**
   * Create an empty document to circumvant the lack of any WindowGlobal/document
   * running for this addon.
   *
   * For now DevTools always expect at least one Target to be functional,
   * and we need a document to spawn a target actor.
   */
  async #createFallbackDocument() {
    if (this._browser) {
      return;
    }

    // The extension process browser will only be released on descriptor destruction and can
    // be reused for subsequent watchers if we close and reopen a toolbox from about:debugging.
    //
    // Note that this `getExtensionProcessBrowser` will register the DevTools to the extension codebase.
    // If we stop creating a fallback document, we should register DevTools by some other means.
    this._browser =
      await lazy.ExtensionParent.DebugUtils.getExtensionProcessBrowser(this);

    // As "load" event isn't fired on the <browser> element, use a Web Progress Listener
    // in order to wait for the full loading of that fallback document.
    // It prevents having to deal with the initial about:blank document in the content processes.
    // We have various checks to identify the fallback document based on its URL.
    // It also ensure that the fallback document is created before the watcher starts
    // and helps spawning the target for that document first.
    const onLocationChanged = new Promise(resolve => {
      const listener = {
        onLocationChange: () => {
          this._browser.webProgress.removeProgressListener(listener);
          resolve();
        },
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };

      this._browser.webProgress.addProgressListener(
        listener,
        Ci.nsIWebProgress.NOTIFY_LOCATION
      );
    });

    // Add the addonId in the URL to retrieve this information in other devtools
    // helpers. The addonId is usually populated in the principal, but this will
    // not be the case for the fallback window because it is loaded from chrome://
    // instead of moz-extension://${addonId}
    this._browser.setAttribute(
      "src",
      `${WEBEXTENSION_FALLBACK_DOC_URL}#${this.addonId}`
    );
    await onLocationChanged;
  }

  /**
   * Note that reloadDescriptor is the common API name for descriptors
   * which support to be reloaded, while WebExtensionDescriptorActor::reload
   * is a legacy API which is for instance used from web-ext.
   *
   * bypassCache has no impact for addon reloads.
   */
  reloadDescriptor() {
    return this.reload();
  }

  async reload() {
    await this.addon.reload();
    return {};
  }

  async terminateBackgroundScript() {
    await lazy.ExtensionParent.DebugUtils.terminateBackgroundScript(
      this.addonId
    );
  }

  // This function will be called from RootActor in case that the devtools client
  // retrieves list of addons with `iconDataURL` option.
  async loadIconDataURL() {
    this._iconDataURL = await this.getIconDataURL();
  }

  async getIconDataURL() {
    if (!this.addon.iconURL) {
      return null;
    }

    const xhr = new XMLHttpRequest();
    xhr.responseType = "blob";
    xhr.open("GET", this.addon.iconURL, true);

    if (this.addon.iconURL.toLowerCase().endsWith(".svg")) {
      // Maybe SVG, thus force to change mime type.
      xhr.overrideMimeType("image/svg+xml");
    }

    try {
      const blob = await new Promise((resolve, reject) => {
        xhr.onload = () => resolve(xhr.response);
        xhr.onerror = reject;
        xhr.send();
      });

      const reader = new FileReader();
      return await new Promise((resolve, reject) => {
        reader.onloadend = () => resolve(reader.result);
        reader.onerror = reject;
        reader.readAsDataURL(blob);
      });
    } catch (_) {
      console.warn(`Failed to create data url from [${this.addon.iconURL}]`);
      return null;
    }
  }

  // Private Methods
  _getBackgroundScriptStatus() {
    const isRunning = lazy.ExtensionParent.DebugUtils.isBackgroundScriptRunning(
      this.addonId
    );
    // The background script status doesn't apply to this addon (e.g. the addon
    // type doesn't have any code, like staticthemes/langpacks/dictionaries, or
    // the extension does not have a background script at all).
    if (isRunning === undefined) {
      return undefined;
    }

    return isRunning ? BGSCRIPT_STATUSES.RUNNING : BGSCRIPT_STATUSES.STOPPED;
  }

  // AddonManagerListener callbacks.
  onInstalled(addon) {
    if (addon.id != this.addonId) {
      return;
    }

    // Update the AddonManager's addon object on reload/update.
    this.addon = addon;
  }

  onUninstalled(addon) {
    if (addon != this.addon) {
      return;
    }

    this.destroy();
  }

  destroy() {
    lazy.AddonManager.removeAddonListener(this);

    this.addon = null;

    if (this.watcher) {
      this.watcher = null;
    }

    if (this._browser) {
      lazy.ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(this);
      this._browser = null;
    }

    this.emit("descriptor-destroyed");

    super.destroy();
  }
}

exports.WebExtensionDescriptorActor = WebExtensionDescriptorActor;
