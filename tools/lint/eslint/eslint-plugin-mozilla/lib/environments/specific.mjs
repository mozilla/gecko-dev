/**
 * @file Defines the environment for the Firefox browser. Allows global
 *               variables which are non-standard and specific to Firefox.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export default {
  globals: {
    Cc: "readonly",
    ChromeUtils: "readonly",
    Ci: "readonly",
    Components: "readonly",
    Cr: "readonly",
    Cu: "readonly",
    Debugger: "readonly",
    InstallTrigger: "readonly",
    // https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/InternalError
    InternalError: "writeable",
    // https://github.com/mozilla/explainers/MessagingLayerSecurity.md
    MLS: "readonly",
    Services: "readonly",
    // https://developer.mozilla.org/docs/Web/API/Window/dump
    dump: "writeable",
    openDialog: "readonly",
    // https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/uneval
    uneval: "readonly",
  },
};
