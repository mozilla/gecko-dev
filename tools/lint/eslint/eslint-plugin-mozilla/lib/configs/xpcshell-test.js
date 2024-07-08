// Parent config file for all xpcshell files.
"use strict";

module.exports = {
  env: {
    browser: false,
    "mozilla/privileged": true,
    "mozilla/xpcshell": true,
  },

  overrides: [
    {
      // Some directories have multiple kinds of tests, and some rules
      // don't work well for plain mochitests, so disable those.
      files: ["*.html", "*.xhtml"],
      // plain/chrome mochitests don't automatically include Assert, so
      // autofixing `ok()` to Assert.something is bad.
      rules: {
        "mozilla/no-comparison-or-assignment-inside-ok": "off",
      },
    },
    {
      // If it is a head file, we turn off global unused variable checks, as it
      // would require searching the other test files to know if they are used or not.
      // This would be expensive and slow, and it isn't worth it for head files.
      // We could get developers to declare as exported, but that doesn't seem worth it.
      files: "head*.js",
      rules: {
        "no-unused-vars": [
          "error",
          {
            argsIgnorePattern: "^_",
            vars: "local",
          },
        ],
      },
    },
    {
      // No declaring variables that are never used
      files: "test*.js",
      rules: {
        "no-unused-vars": [
          "error",
          {
            argsIgnorePattern: "^_",
            vars: "all",
          },
        ],
      },
    },
  ],
  plugins: ["mozilla", "@microsoft/sdl"],

  rules: {
    // Turn off no-insecure-url as it is not considered necessary for xpcshell
    // level tests.
    "@microsoft/sdl/no-insecure-url": "off",

    "mozilla/no-comparison-or-assignment-inside-ok": "error",
    "mozilla/no-useless-run-test": "error",
    "no-shadow": "error",
  },
};
