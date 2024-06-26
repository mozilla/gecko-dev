/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `border-spacing` test cases.
export default [
  {
    info: "border-spacing is inactive on a table with collapsed borders",
    property: "border-spacing",
    tagName: "table",
    rules: ["table { border-collapse: collapse; border-spacing: 10px; }"],
    isActive: false,
  },
  {
    info: "border-spacing is active on a table with separated borders",
    property: "border-spacing",
    tagName: "table",
    rules: ["table { border-spacing: 10px; }"],
    isActive: true,
  },
];
