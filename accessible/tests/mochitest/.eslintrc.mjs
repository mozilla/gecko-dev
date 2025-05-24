/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default [
  {
    rules: {
      // XXX These are rules that are enabled in the recommended configuration, but
      // disabled here due to failures when initially implemented. They should be
      // removed (and hence enabled) at some stage.
      "no-nested-ternary": "off",
    },
  },
  {
    files: [
      // Bug 1602061 TODO: These tests access DOM elements via
      // id-as-variable-name, which eslint doesn't have support for yet.
      "attributes/test_listbox.html",
      "treeupdate/test_ariaowns.html",
    ],
    rules: {
      "no-undef": "off",
    },
  },
];
