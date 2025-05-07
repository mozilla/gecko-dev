/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1919263 - nbcsports.com videos and photos are not displayed
 *
 * The site loads a script, mparticle.js, using a script tag with async=true,
 * but this can cause it to load too late for other scripts on the page. We
 * can prevent this by changing async to false when they try to load mparticle.js.
 */

/* globals exportFunction */

console.info(
  "mparticle.js is being kept from loading async. See https://bugzilla.mozilla.org/show_bug.cgi?id=1919263 for details."
);

const { prototype } = HTMLScriptElement.wrappedJSObject;
const desc = Object.getOwnPropertyDescriptor(prototype, "src");
const origSet = desc.set;
desc.set = exportFunction(function (url) {
  if (url?.includes("mparticle.js")) {
    this.async = false;
  }
  return origSet.call(this, url);
}, window);
Object.defineProperty(prototype, "src", desc);
