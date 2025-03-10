/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * www.capital.gr - Reloading the page results in endless reloading loops.
 * Bug #1943898 - https://bugzilla.mozilla.org/show_bug.cgi?id=1943898
 * WebCompat issue #74073 - https://webcompat.com/issues/74073
 *
 * The site does not anticipate Firefox's form-filling behavior on page
 * reloads, which causes it to accidentally trigger endless reload loops.
 * We fix this by clearing the form element as they check it the first time.
 */

/* globals exportFunction */

console.info(
  "setTimeout has been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1943898 for details."
);

Object.defineProperty(window.wrappedJSObject, "setTimeout", {
  value: exportFunction(function (fn, time) {
    const text = "" + fn;
    if (
      text.includes("var el = document.getElementById('alwaysFetch');") &&
      text.includes("el.value = el.value ? location.reload() : true;")
    ) {
      document.getElementById("alwaysFetch").value = "";
    }
    return window.setTimeout(fn, time);
  }, window),
});
