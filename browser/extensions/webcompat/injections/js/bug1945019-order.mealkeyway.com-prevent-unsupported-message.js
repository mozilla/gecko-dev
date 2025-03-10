/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals exportFunction */

"use strict";

/**
 * www.menusifu.com - Shows an 'for a better experience, use Chrome' message.
 * Bug #1945019 - https://bugzilla.mozilla.org/show_bug.cgi?id=1945019
 *
 * We can automatically hide the message for users.
 */

console.info(
  "Web compatibility fixes are in effect. See https://bugzilla.mozilla.org/show_bug.cgi?id=1923286 for details."
);

const timer = setInterval(() => {
  for (const node of document.querySelectorAll(
    "[class*=customMiniPrompt_customPromptContent]"
  )) {
    if (node.innerText.includes("Chrome")) {
      node.parentElement
        .querySelector("[class*=customMiniPrompt_iconAdd]")
        ?.click();
      clearInterval(timer);
    }
  }
}, 100);
