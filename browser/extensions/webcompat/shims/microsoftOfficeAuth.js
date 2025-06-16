/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1747889 - Microsoft Office Auth
 *
 * The Microsoft Office auth iframe is missing the sandbox attribute
 * 'allow-storage-access-by-user-activation'. This shim adds the attribute to
 * the iframe.
 */

const SANDBOX_ATTR = "allow-storage-access-by-user-activation";

// Watches for MS auth iframes and adds missing sandbox attribute.
function init() {
  const observer = new MutationObserver(() => {
    document.body.querySelectorAll("#SharedAuthFrame").forEach(frame => {
      frame.sandbox.add(SANDBOX_ATTR);
    });
  });

  observer.observe(document.body, {
    attributes: true,
    subtree: false,
    childList: true,
  });
}
window.addEventListener("DOMContentLoaded", init);
