/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

console.info(
  "Links with javascript: URLs are being ammended to prevent breakage. See https://bugzilla.mozilla.org/show_bug.cgi?id=1928954 for details."
);

document.documentElement.addEventListener(
  "click",
  e => {
    const jslink = e.target?.closest("a[href^='javascript:']");
    if (jslink && !jslink.href.endsWith(";undefined")) {
      jslink.href = `${jslink.href};undefined`;
    }
  },
  true
);
