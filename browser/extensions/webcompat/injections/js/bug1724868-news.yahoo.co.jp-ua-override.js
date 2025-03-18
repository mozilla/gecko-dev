/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1724868 - news.yahoo.co.jp - Override UA on Android and Linux
 * WebCompat issue #82605 - https://webcompat.com/issues/82605
 *
 * Yahoo Japan news doesn't allow playing video in Firefox on Android or Linux
 * as those are not in their support matrix. They check UA override twice
 * and display different UI with the same error. Changing the UA to Chrome via
 * content script allows playing the videos.
 */

/* globals exportFunction, UAHelpers */

console.info(
  "The user agent has been overridden for compatibility reasons. See https://webcompat.com/issues/82605 for details."
);

UAHelpers.overrideWithDeviceAppropriateChromeUA({ OS: "nonLinux" });
