// Parent config file for all xpcshell files.
"use strict";

module.exports = {
  env: {
    browser: false,
    "mozilla/privileged": true,
    "mozilla/xpcshell": true,
  },

  plugins: ["mozilla", "@microsoft/sdl"],

  rules: {
    // Turn off no-insecure-url as it is not considered necessary for xpcshell
    // level tests.
    "@microsoft/sdl/no-insecure-url": "off",

    "mozilla/no-comparison-or-assignment-inside-ok": "error",
    "mozilla/no-useless-run-test": "error",
  },
};
