/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1972859 - Fix blank page at primericaonline.com
 *
 * The site is assuming location.ancestorOrigins is present, and fails to load.
 * We can shim ancestorOrigins to prevent the failure.
 */

/* globals cloneInto */

console.info(
  "location.ancestorOrigins is being shimmed for compatibility reasons. https://bugzilla.mozilla.org/show_bug.cgi?id=1972859 for details."
);

window.wrappedJSObject.Location.prototype.ancestorOrigins = cloneInto(
  [],
  window
);
