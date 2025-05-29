/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Parent config file for all xpcshell files.

export default {
  env: {
    "mozilla/privileged": true,
    "mozilla/xpcshell": true,
  },

  name: "mozilla/xpcshell-test",
  plugins: ["mozilla", "@microsoft/sdl"],

  rules: {
    // Turn off no-insecure-url as it is not considered necessary for xpcshell
    // level tests.
    "@microsoft/sdl/no-insecure-url": "off",

    "mozilla/no-comparison-or-assignment-inside-ok": "error",
    "mozilla/no-useless-run-test": "error",
  },
};
