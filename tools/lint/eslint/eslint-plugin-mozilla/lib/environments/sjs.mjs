/**
 * @file Defines the environment for sjs files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export default {
  globals: {
    // All these variables are hard-coded to be available for sjs scopes only.
    // https://searchfox.org/mozilla-central/rev/26a1b0fce12e6dd495a954c542bb1e7bd6e0d548/netwerk/test/httpserver/httpd.js#2879
    atob: "readonly",
    btoa: "readonly",
    Cc: "readonly",
    ChromeUtils: "readonly",
    Ci: "readonly",
    Components: "readonly",
    Cr: "readonly",
    Cu: "readonly",
    dump: "readonly",
    IOUtils: "readonly",
    PathUtils: "readonly",
    TextDecoder: "readonly",
    TextEncoder: "readonly",
    URLSearchParams: "readonly",
    URL: "readonly",
    getState: "readonly",
    setState: "readonly",
    getSharedState: "readonly",
    setSharedState: "readonly",
    getObjectState: "readonly",
    setObjectState: "readonly",
    registerPathHandler: "readonly",
    Services: "readonly",
    // importScripts is also available.
    importScripts: "readonly",
  },
};
