/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const path = require("path");
const fs = require("fs");

const PREFIX = "moz-src:///";

exports.interfaceVersion = 2;
exports.resolve = source => {
  if (!source.startsWith(PREFIX)) {
    return { found: false };
  }

  let result = path.resolve(
    __dirname,
    ...source.substring(PREFIX.length).split("/")
  );
  let stats = fs.statSync(result, { throwIfNoEntry: false });
  if (!stats || !stats.isFile()) {
    return { found: false };
  }

  return {
    found: true,
    path: result,
  };
};
