/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1944725 - window.innerWidth/Height fix
 *
 * A white bar appears on the left of the screen at small window sizes, due to
 * window.innerWidth/Height providing different values than the site expects.
 */

/* globals exportFunction */

console.info(
  "window.innerWidth and window.innerHeight have been shimmed for compatibility reasons. https://bugzilla.mozilla.org/show_bug.cgi?id=1944725 for details."
);

const div = document.createElement("div");
div.inert = true;
div.style =
  "position:fixed; top:0; left:0; right:0; bottom:0; opacity:0; z-index:-1";
document.documentElement.appendChild(div);

const win = window.wrappedJSObject;

const iw = Object.getOwnPropertyDescriptor(win, "innerWidth");
iw.get = exportFunction(() => div.getBoundingClientRect().width, window);
Object.defineProperty(win, "innerWidth", iw);

const ih = Object.getOwnPropertyDescriptor(win, "innerHeight");
ih.get = exportFunction(() => div.getBoundingClientRect().height, window);
Object.defineProperty(win, "innerHeight", ih);

window.wrappedJSObject.chrome = new window.wrappedJSObject.Object();
