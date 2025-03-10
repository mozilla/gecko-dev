/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

// JS files which are needed by browser.xhtml but no other top level windows to
// support MacOS specific features should be loaded directly from browser-main.js
// rather than this file.
//
// If you update this list, you may need to add a mapping within the following
// file so that ESLint works correctly:
// tools/lint/eslint/eslint-plugin-mozilla/lib/environments/browser-window.js

// prettier-ignore
// eslint-disable-next-line no-lone-blocks
{
  Services.scriptloader.loadSubScript("chrome://browser/content/browser.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/places/browserPlacesViews.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/browser-places.js", this);
  Services.scriptloader.loadSubScript("chrome://global/content/globalOverlay.js", this);
  Services.scriptloader.loadSubScript("chrome://global/content/editMenuOverlay.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/utilityOverlay.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/browser-sets.js", this);
  if (AppConstants.platform == "macosx") {
    Services.scriptloader.loadSubScript("chrome://global/content/macWindowMenu.js", this);
  }
}
