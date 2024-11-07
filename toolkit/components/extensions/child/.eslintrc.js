/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  globals: {
    // These are defined in the WebExtension script scopes by
    // ExtensionCommon.sys.mjs in the _createExtGlobal method.
    AppConstants: true,
    Cc: true,
    ChromeWorker: true,
    Ci: true,
    Cr: true,
    Cu: true,
    ExtensionAPI: true,
    ExtensionAPIPersistent: true,
    ExtensionCommon: true,
    FileReader: true,
    Glean: true,
    GleanPings: true,
    IOUtils: true,
    MatchGlob: true,
    MatchPattern: true,
    MatchPatternSet: true,
    OffscreenCanvas: true,
    PathUtils: true,
    Services: true,
    StructuredCloneHolder: true,
    WebExtensionPolicy: true,
    XPCOMUtils: true,
    extensions: true,
    global: true,
    ExtensionUtils: true,

    // This is defined in toolkit/components/extensions/child/ext-toolkit.js
    EventManager: true,
  },
};
