/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1899937 - Shim requestPictureInPicture for plus.nhk.jp
 * WebCompat issue #103463 - https://webcompat.com/issues/103463
 *
 * plus.nhk.jp is showing an error when attempting to play videos.
 * Shimming requestPictureInPicture to `{}` makes the videos play.
 */

/* globals exportFunction, cloneInto */

console.info(
  "PictureInPicture APIs are being shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1899937 for details."
);

const win = window.wrappedJSObject;

const proto = win.HTMLVideoElement.prototype;
const promise = win.Promise.resolve();
proto.requestPictureInPicture = exportFunction(() => promise, window);

win.Document.prototype.pictureInPictureElement = null;
win.PictureInPictureWindow = exportFunction(class {}, window);
win.PictureInPictureEvent = exportFunction(class {}, window);
