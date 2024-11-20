/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * www.bing.com - Clicking on images from others pages does nothing
 *
 * When viewing images from other pages in image search results,
 * clicking on them does nothing. We can intercept the click
 * attempts and force the navigation as a work-around.
 */

console.info(
  "Clicking on image links is being emulated for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1923286 for details."
);

document.addEventListener("click", ({ target }) => {
  const link = target?.closest(".richImgLnk[target=_blank]");
  if (link) {
    const a = document.createElement("a");
    a.target = "_blank";
    a.href = link.href;
    a.click();
  }
});
