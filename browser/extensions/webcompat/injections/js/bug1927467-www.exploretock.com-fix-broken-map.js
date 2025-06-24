/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1927467 - Fix for broken maps on www.exploretock.com
 *
 * The site is using an outdated Google Maps version which incorrectly assumes
 * that `new Worker` will throw an exception for blocked URLs, when it's
 * unclear what should really happen per spec. This shim throws to fix it.
 */

/* globals exportFunction */

console.info(
  "Worker constructor is being shimmed for compatibility reasons. https://bugzilla.mozilla.org/show_bug.cgi?id=1927467 for details."
);

const win = window.wrappedJSObject;
const winWorker = win.Worker;
win.Worker = exportFunction(function (url, opts) {
  if (url.startsWith("blob:")) {
    throw new win.Error(`Could not create Worker for ${url}`);
  }
  return winWorker.call(this, url, opts);
}, window);
