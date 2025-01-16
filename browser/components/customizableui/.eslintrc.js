/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.exports = {
  // When adding items to this file please check for effects on sub-directories.
  overrides: [
    {
      // These files use fluent-dom to insert content
      files: ["content/panelUI.js", "test/**"],
      rules: {
        "jsdoc/check-access": "off",
        "jsdoc/check-param-names": "off",
        "jsdoc/check-property-names": "off",
        "jsdoc/check-tag-names": "off",
        "jsdoc/check-types": "off",
        "jsdoc/empty-tags": "off",
        "jsdoc/no-multi-asterisks": "off",
        "jsdoc/require-param-type": "off",
        "jsdoc/require-returns-type": "off",
        "jsdoc/tag-lines": "off",
        "jsdoc/valid-types": "off",
      },
    },
  ],
};
