/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

/**
 * Bug 1934814 - Messenger login broken with Total Cookie Protection
 *
 * The messenger login flow redirects to the Facebook page and then back to the
 * messenger page to finish the login. However, the redirect could get stuck in
 * the Facebook page for unknown reasons.
 *
 * This shim requests storage access for Facebook under the messenger page to
 * allow Facebook SSO login to work on the messenger page. So, there will be
 * no redirection and fix the login issue.
 */

console.warn(
  `When logging in, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1934814 for details.`
);

const STORAGE_ACCESS_ORIGIN = "https://www.facebook.com";

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }
    const button = target.closest("button[id=loginbutton]");
    if (!button) {
      return;
    }

    // We don't need to do anything if the button is not visible. When the login
    // button is hidden, the Facebook SSO login button is shown instead. In this
    // case, we don't need to do anything.
    if (
      !button.checkVisibility({
        contentVisibilityAuto: true,
        opacityProperty: true,
        visibilityProperty: true,
      })
    ) {
      return;
    }

    console.warn(
      "Calling the Storage Access API on behalf of " + STORAGE_ACCESS_ORIGIN
    );
    button.disabled = true;
    e.stopPropagation();
    e.preventDefault();
    document
      .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
      .then(_ => {
        return browser.runtime.sendMessage({
          message: "checkFacebookLoginStatus",
          shimId: "MessengerLogin",
        });
      })
      .then(isLoggedIn => {
        button.disabled = false;

        if (!isLoggedIn) {
          // We need to click the login button to continue the login flow if
          // the user is not logged in to Facebook.
          button.click();
        } else {
          // Reload the page so that the messenger page will show Facebook SSO
          // button instead of the login button.
          location.reload();
        }
      })
      .catch(() => {
        button.disabled = false;
        // Continue the login flow if the storage access is denied by clicking
        // the button again.
        button.click();
      });
  },
  true
);
