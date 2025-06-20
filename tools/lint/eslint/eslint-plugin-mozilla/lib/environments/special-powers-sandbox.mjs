/**
 * @file Defines the environment for SpecialPowers sandbox.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export default {
  globals: {
    // wantComponents defaults to true,
    Components: "readonly",
    Ci: "readonly",
    Cr: "readonly",
    Cc: "readonly",
    Cu: "readonly",
    Services: "readonly",

    // testing/specialpowers/content/SpecialPowersSandbox.sys.mjs

    // SANDBOX_GLOBALS
    Blob: "readonly",
    ChromeUtils: "readonly",
    FileReader: "readonly",
    TextDecoder: "readonly",
    TextEncoder: "readonly",
    URL: "readonly",

    // EXTRA_IMPORTS
    EventUtils: "readonly",

    // SpecialPowersSandbox constructor
    assert: "readonly",
    Assert: "readonly",
    BrowsingContext: "readonly",
    InspectorCSSParser: "readonly",
    InspectorUtils: "readonly",
    ok: "readonly",
    is: "readonly",
    isnot: "readonly",
    todo: "readonly",
    todo_is: "readonly",
    info: "readonly",
  },
};
