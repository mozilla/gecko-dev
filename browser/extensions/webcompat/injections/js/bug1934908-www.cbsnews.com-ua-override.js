/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1934908 - www.cbsnews.com - Override UA on Android to use JS HLS player
 * Spoofing as Chrome forces the site’s HLS.js player. Native HLS is controlled
 * by media.hls.enabled, which defaults to false. When false, video doesn't
 * play back at all. When true, video will play back for a minute or two before
 * stopping. With this override video playback works consistently regardless of
 * how media.hls.enabled is configured.
 */

/* globals exportFunction, UAHelpers */
console.info(
  "The user agent has been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1934908 for details."
);

UAHelpers.overrideWithDeviceAppropriateChromeUA();
const nav = Object.getPrototypeOf(navigator.wrappedJSObject);
const vendorDesc = Object.getOwnPropertyDescriptor(nav, "vendor");
vendorDesc.get = exportFunction(() => "Google Inc.", window);
Object.defineProperty(nav, "vendor", vendorDesc);
