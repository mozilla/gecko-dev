/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1841730 - Fix www.korg.com support download page loads on Windows.
 *
 * They are using a library named PACE, which has a timing bug with Firefox
 * which breaks page loads (due to a stuck progress indicator) on Windows.
 * This is the fix suggested at https://github.com/CodeByZach/pace/issues/510
 */

/* globals cloneInto */

window.wrappedJSObject.paceOptions = cloneInto({ eventLag: false }, window);
