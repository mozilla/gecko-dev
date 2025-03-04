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

const ResourceSubstitution = "newtab";

this.resourceMapping = class extends ExtensionAPI {
  #manifest = null;

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
    if (this.extension.rootURI instanceof Ci.nsIJARURI) {
      this.#manifest = this.extension.rootURI.JARFile.QueryInterface(
        Ci.nsIFileURL
      ).file;
    } else if (this.extension.rootURI instanceof Ci.nsIFileURL) {
      this.#manifest = this.extension.rootURI.file;
    }

    Components.manager.addBootstrappedManifestLocation(this.#manifest);
  }

  onShutdown() {
    if (!AppConstants.BROWSER_NEWTAB_AS_ADDON) {
      // See the note in onStartup for why we're bailing out here.
      return;
    }

    resProto.setSubstitution(ResourceSubstitution, null);
    Components.manager.removeBootstrappedManifestLocation(this.#manifest);
    this.#manifest = null;
  }
};
