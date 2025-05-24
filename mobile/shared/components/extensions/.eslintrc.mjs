/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import toolkitConfig from "../../../../toolkit/components/extensions/.eslintrc.mjs";
import parentConfig from "../../../../toolkit/components/extensions/parent/.eslintrc.mjs";
import childConfig from "../../../../toolkit/components/extensions/child/.eslintrc.mjs";

export default [
  ...toolkitConfig,
  // Ideally mobile should also follow the convention of
  // parent/ext-*.js for parent scripts and
  // child/ext-*.js for child scripts,
  // but the current file structure predates the parent/ vs child/ separation.
  {
    files: ["ext-*.js"],
    ignores: ["ext-c-*.js"],
    languageOptions: {
      globals: {
        ...parentConfig[0].languageOptions.globals,
        // These globals are defined in ext-android.js and can only be used in
        // the extension files that run in the parent process.
        EventDispatcher: true,
        ExtensionError: true,
        makeGlobalEvent: true,
        TabContext: true,
        tabTracker: true,
        windowTracker: true,
      },
    },
  },
  {
    files: ["ext-c-*.js"],
    // If there were ever globals exported in ext-c-android.js for common
    // use, then they would appear here.
    ...childConfig[0],
  },
];
