/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ViewportSizeHighlighter,
} = require("resource://devtools/server/actors/highlighters/viewport-size.js");

const HIDE_TIMEOUT_MS = Services.prefs.getIntPref(
  "devtools.highlighter-viewport-size-timeout",
  1000
);

/**
 * The ViewportSizeOnResizeHighlighter is a class that displays the viewport
 * width and height when the viewport size resizes, even when the rulers are not displayed.
 */
class ViewportSizeOnResizeHighlighter extends ViewportSizeHighlighter {
  constructor(highlighterEnv, parent) {
    super(highlighterEnv, parent, {
      prefix: "viewport-size-on-resize-highlighter-",
      hideTimeout: HIDE_TIMEOUT_MS,
      waitForDocumentToLoad: false,
    });
  }
}
exports.ViewportSizeOnResizeHighlighter = ViewportSizeOnResizeHighlighter;
