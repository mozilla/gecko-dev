/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global ExtensionAPI, XPCOMUtils, Services */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AboutHomeStartupCache: "resource:///modules/AboutHomeStartupCache.sys.mjs",
});

const ResourceSubstitution = "newtab";

this.builtin_newtab = class extends ExtensionAPI {
  #chromeHandle = null;

  onStartup() {
    if (!AppConstants.BROWSER_NEWTAB_AS_ADDON) {
      // If we're here, this must be the first launch of a profile where this
      // addon had been previously installed, and the uninstall hasn't
      // completed yet. In that case, let's just do nothing, and not interfere
      // with the component version of newtab that we're now configured to use.
      return;
    }

    const { rootURI } = this.extension;

    resProto.setSubstitutionWithFlags(
      ResourceSubstitution,
      rootURI,
      Ci.nsISubstitutingProtocolHandler.ALLOW_CONTENT_ACCESS
    );

    let aomStartup = Cc[
      "@mozilla.org/addons/addon-manager-startup;1"
    ].getService(Ci.amIAddonManagerStartup);
    const manifestURI = Services.io.newURI(
      "manifest.json",
      null,
      this.extension.rootURI
    );
    this.#chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "newtab", "data/content", "contentaccessible=yes"],
    ]);

    let redirector = Cc[
      "@mozilla.org/network/protocol/about;1?what=newtab"
    ].getService(Ci.nsIAboutModule).wrappedJSObject;
    redirector.builtInAddonInitialized();
  }

  onShutdown() {
    if (!AppConstants.BROWSER_NEWTAB_AS_ADDON) {
      // See the note in onStartup for why we're bailing out here.
      return;
    }

    resProto.setSubstitution(ResourceSubstitution, null);
    this.#chromeHandle.destruct();
    this.#chromeHandle = null;
  }

  getAPI(_context) {
    return {
      builtin: {
        newtab: {
          handleUpdateAvailable() {
            // An update was downloaded, but will not apply until restart. If
            // we attempt to restart the browser, any cached about:home might
            // not be compatible with the newtab code from the updated add-on.
            // We invalidate the cache here, and uninit the
            // AboutHomeStartupCache so that it won't attempt to recreate the
            // cache until after the next restart.
            lazy.AboutHomeStartupCache.clearCacheAndUninit();
          },
        },
      },
    };
  }
};
