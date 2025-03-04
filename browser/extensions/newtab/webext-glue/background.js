/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env webextensions */

"use strict";

browser.runtime.onUpdateAvailable.addListener(_details => {
  // The New Tab built-in add-on is not designed to be updated at runtime.
  //
  // By listening to but ignoring this event, any updates will
  // be delayed until the next browser restart.
  //
  // We also need to invalidate the AboutHomeStartupCache when an update is
  // available, and prevent any new caches from being created until the
  // next browser restart. That's covered in bug 1946564.
});
