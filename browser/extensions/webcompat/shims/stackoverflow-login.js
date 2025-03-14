/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

console.warn(
  `When logging in, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1949491 for details.`
);

const STORAGE_ACCESS_ORIGIN = "https://stackexchange.com";

const STACKOVERFLOW_LOGIN_HERF_PREFIX = "https://stackoverflow.com/users/login";
const STACKOVERFLOW_SIGNUP_HERF_PREFIX =
  "https://stackoverflow.com/users/signup";

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }

    // Find the click event on button elements.
    const button = target.closest("a.s-btn");
    if (!button) {
      return;
    }

    // Only request storage access for login/signup buttons.
    if (
      !button.href.startsWith(STACKOVERFLOW_LOGIN_HERF_PREFIX) &&
      !button.href.startsWith(STACKOVERFLOW_SIGNUP_HERF_PREFIX)
    ) {
      return;
    }

    console.warn(
      "Calling the Storage Access API on behalf of " + STORAGE_ACCESS_ORIGIN
    );

    // Disable the button to prevent it from being clicked again.
    button.style.pointerEvents = "none";
    e.stopPropagation();
    e.preventDefault();
    document
      .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
      .finally(() => {
        // Re-enable the button and proceed with the login/signup flow.
        button.style.pointerEvents = "";
        button.click();
      });
  },
  true
);
