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

    // These are defined in toolkit/components/extensions/parent/ext-tabs-base.js
    TabBase: true,
    TabManagerBase: true,
    TabTrackerBase: true,
    WindowBase: true,
    WindowManagerBase: true,
    WindowTrackerBase: true,
    getUserContextIdForCookieStoreId: true,
    // There are defined in toolkit/components/extensions/parent/ext-toolkit.js
    CONTAINER_STORE: true,
    DEFAULT_STORE: true,
    EventEmitter: true,
    EventManager: true,
    PRIVATE_STORE: true,
    getContainerForCookieStoreId: true,
    getCookieStoreIdForContainer: true,
    getCookieStoreIdForOriginAttributes: true,
    getCookieStoreIdForTab: true,
    getOriginAttributesPatternForCookieStoreId: true,
    isContainerCookieStoreId: true,
    isDefaultCookieStoreId: true,
    isPrivateCookieStoreId: true,
    isValidCookieStoreId: true,
  },
};
