/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `resize` test cases.
export default [
  {
    info: "resize is inactive on non-overflowing element",
    property: "resize",
    tagName: "div",
    rules: ["div { resize: both; }"],
    isActive: false,
  },
  {
    info: "resize is active on overflowing element",
    property: "resize",
    tagName: "div",
    rules: [
      "div { resize: both; width: 50px; height: 50px; overflow: scroll; }",
    ],
    isActive: true,
  },
  {
    info: "resize is inactive on element with overflow: visible",
    property: "resize",
    tagName: "div",
    rules: ["div { resize: both; overflow: visible; }"],
    isActive: false,
  },
  {
    info: "resize is inactive on input element",
    property: "resize",
    tagName: "input",
    rules: ["input { resize: both; }"],
    isActive: false,
  },
  {
    info: "resize is active on textarea element",
    property: "resize",
    tagName: "textarea",
    rules: ["textarea { resize: both; }"],
    isActive: true,
  },
  // This has to be changed once bug 680823 is fixed.
  {
    info: "resize is inactive on iframe element",
    property: "resize",
    tagName: "iframe",
    rules: ["iframe { resize: both; }"],
    isActive: false,
  },
  // This has to be changed once bug 1280920 or its dependencies are fixed.
  {
    info: "resize is inactive on img element",
    property: "resize",
    tagName: "img",
    rules: ["img { resize: both; }"],
    isActive: false,
  },
  // This has to be changed once bug 1280920 or its dependencies are fixed.
  {
    info: "resize is inactive on picture element",
    property: "resize",
    tagName: "picture",
    rules: ["picture { resize: both; }"],
    isActive: false,
  },
  // This has to be changed once bug 1280920 or its dependencies are fixed.
  {
    info: "resize is inactive on svg element",
    property: "resize",
    tagName: "svg",
    rules: ["svg { resize: both; }"],
    isActive: false,
  },
  // This has to be changed once bug 1280920 or its dependencies are fixed.
  {
    info: "resize is inactive on canvas element",
    property: "resize",
    tagName: "canvas",
    rules: ["canvas { resize: both; }"],
    isActive: false,
  },
  // This has to be changed once bug 1280920 or its dependencies are fixed.
  {
    info: "resize is inactive on video element",
    property: "resize",
    tagName: "video",
    rules: ["video { resize: both; }"],
    isActive: false,
  },
  // This has to be changed once bug 1280920 or its dependencies are fixed.
  {
    info: "resize is inactive on object element",
    property: "resize",
    tagName: "object",
    rules: ["object { resize: both; }"],
    isActive: false,
  },
];
