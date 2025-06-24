/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1939466 - Fix "No right to access page!" error on vodafone.it
 *
 * The site has a bug with its frame-ancestor code for Firefox, which we can
 * bypass by undefining InstallTrigger and spoofing location.ancestorOrigins.
 */

/* globals cloneInto */

console.info(
  "location.ancestorOrigins and InstallTrigger are being shimmed for compatibility reasons. https://bugzilla.mozilla.org/show_bug.cgi?id=1939466 for details."
);

delete window.wrappedJSObject.InstallTrigger;

window.wrappedJSObject.Location.prototype.ancestorOrigins = cloneInto(
  ["https://www.vodafone.it"],
  window
);
console.error(location, location.ancestorOrigins);
