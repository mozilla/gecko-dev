/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1967510 - gemini.google.com copy-pasting between inputs does not work
 *
 * When editing a prompt and copying some text, they call setData with an empty string
 * for text/html. Firefox dutifully sets that value, other browsers ignore it. Then when
 * trying to paste, they check if it's an empty string and ignore it, so pasting the value
 * does nothing. We can work around this interop quirk by just clearing that empty data.
 */

/* globals exportFunction, cloneInto */

console.info(
  "Overriding DataTransfer.setData compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1967510 for details."
);

const { prototype } = DataTransfer.wrappedJSObject;
const desc = Object.getOwnPropertyDescriptor(prototype, "setData");
const { value } = desc;
desc.value = exportFunction(function (format, data) {
  value(format, data);
  if (data === "") {
    this.clearData("text/html");
  }
}, window);
Object.defineProperty(prototype, "setData", desc);
