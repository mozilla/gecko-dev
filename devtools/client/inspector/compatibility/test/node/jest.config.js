/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global __dirname */

module.exports = {
  verbose: true,
  moduleNameMapper: {
    // Map all require("devtools/...") to the real devtools root.
    "^devtools\\/(.*)": `${__dirname}/../../../../../$1`,
    "^Services": `${__dirname}/fixtures/Services`,
  },
  setupFiles: ["<rootDir>setup.js"],
  snapshotSerializers: ["enzyme-to-json/serializer"],
};
