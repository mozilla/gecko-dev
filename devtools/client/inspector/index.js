/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

var isInChrome = window.location.protocol == "chrome:";
if (isInChrome) {
  /* eslint-disable */
  var exports = {};
  var { require, loader } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/Loader.sys.mjs"
  );
  var { BrowserLoader } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/browser-loader.sys.mjs"
  );
}
