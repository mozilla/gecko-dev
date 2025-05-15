/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1963270 - app.kosmi.io - missing "local file" media selection option
 *
 * The site does not show its "local file" option when selecting media
 * unless the captureStream API is available, and does not check for the
 * prefixed version (which it seems to work with, so we unprefix it here).
 */

const { prototype } = HTMLMediaElement.wrappedJSObject;
prototype.captureStream = prototype.mozCaptureStream;
