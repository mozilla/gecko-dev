/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1962353 - Fix www.cartlow.com product filters
 * WebCompat issue #152573 - https://webcompat.com/issues/152573
 *
 * The site replaces their filter form elements in a way which is not
 * interoperable on Chrome and Firefox. By removing the `for` attribute
 * from the related labels, it works as expected.
 */

console.info(
  "HTML 'for' attributes are being removed from product filter labels for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1962353 for details."
);

const changedContainerId = "condition-filter";
const labelForQuery = "label.custom-control-label[for]";

const labelForObserver = new MutationObserver(mutations => {
  for (let { addedNodes, target } of mutations) {
    if (target.id !== changedContainerId) {
      continue;
    }
    for (const node of addedNodes) {
      node
        .querySelectorAll?.(labelForQuery)
        .forEach(label => label.removeAttribute("for"));
    }
  }
});

labelForObserver.observe(document.documentElement, {
  childList: true,
  subtree: true,
});
