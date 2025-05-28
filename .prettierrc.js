/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = {
  arrowParens: "avoid",
  endOfLine: "lf",
  printWidth: 80,
  tabWidth: 2,
  trailingComma: "es5",
  overrides: [
    {
      files: "*.css",
      options: {
        parser: "css",
        // Using a larger printWidth to avoid wrapping selectors.
        printWidth: 160,
      },
    },
  ],
};
