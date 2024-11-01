/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * bankmandiri.co.id - Define window.chrome to an empty object
 * Bug #1889505 - https://bugzilla.mozilla.org/show_bug.cgi?id=1889505
 * WebCompat issue #67924 - https://webcompat.com/issues/67924
 */

console.info(
  "window.chrome has been set to an empty object for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1889505 for details."
);

window.wrappedJSObject.chrome = new window.wrappedJSObject.Object();
