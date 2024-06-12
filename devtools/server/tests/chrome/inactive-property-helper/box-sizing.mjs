/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `box-sizing` test cases.
export default [
  {
    info: "box-sizing is inactive on inline element",
    property: "box-sizing",
    tagName: "span",
    rules: ["span { box-sizing: border-box; }"],
    isActive: false,
  },
  {
    info: "box-sizing is active on block element",
    property: "box-sizing",
    tagName: "div",
    rules: ["div { box-sizing: border-box; }"],
    isActive: true,
  },
  {
    info: "box-sizing is active on inline-block element",
    property: "box-sizing",
    tagName: "span",
    rules: ["span { display: inline-block; box-sizing: border-box; }"],
    isActive: true,
  },
  {
    info: "box-sizing is active on replaced element",
    property: "box-sizing",
    tagName: "input",
    rules: ["input { display: inline; box-sizing: border-box; }"],
    isActive: true,
  },
];
