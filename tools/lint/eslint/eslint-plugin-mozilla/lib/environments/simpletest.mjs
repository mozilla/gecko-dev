/**
 * @file Defines the environment for scripts that use the SimpleTest
 *               mochitest harness. Imports the globals from the relevant files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

import path from "path";
import { getScriptGlobals } from "./utils.mjs";

// When updating this list, be sure to also update the 'support-files' config
// in `tools/lint/eslint.yml`.
const simpleTestFiles = [
  "AccessibilityUtils.js",
  "ExtensionTestUtils.js",
  "EventUtils.js",
  "GleanTest.js",
  "MockObjects.js",
  "SimpleTest.js",
  "WindowSnapshot.js",
  "paint_listener.js",
];
const simpleTestPath = "testing/mochitest/tests/SimpleTest";

export default getScriptGlobals({
  environmentName: "simpletest",
  files: simpleTestFiles.map(file => path.join(simpleTestPath, file)),
});
