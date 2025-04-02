/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1912228  - aliexpress.com - Unable to change the shipping location back to default
 *
 * Changing the shipping location relies upon access to the unpartitioned cookies for login.aliexpress.com,
 * which are unavailable for localized domains like aliexpress.us. This calls the priveleged Storage Access API
 * for those domains when the user engages with the internationalization UX.
 */

console.warn(
  `When changing languages, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1912228 for details.`
);

// Third-party origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://login.aliexpress.com";

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }
    const i18nButton = target.closest('div[class^="ship-to--menuItem--"]');
    if (!i18nButton) {
      return;
    }

    console.warn(
      "Calling the Storage Access API on behalf of " + STORAGE_ACCESS_ORIGIN
    );
    e.stopPropagation();
    e.preventDefault();
    document
      .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
      .finally(() => {
        target.click();
      });
  },
  true
);
