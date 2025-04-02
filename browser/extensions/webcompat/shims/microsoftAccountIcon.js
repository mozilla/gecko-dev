/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Third-party origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://storage.live.com";

console.warn(
  `Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1728111 for details.`
);

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }

    // If the user clicks the login button, we need to get access for storage.
    const signInButton = target.closest(
      `button[data-bi-id="signedout.hero.signIn"]`
    );

    // Also, fall back to calling it when the user clicks the photo in case there are weird login flows we didn't account for.
    const initialsButton = target.closest(`#O365_MainLink_MePhoto`);

    // Also handle clicks to the sign in dialog on Bing
    const bingSignInButton = target.closest(".id_accountItem");

    // Also handle clicks to the sign in ui on Office.com
    const officeSignInUI = target.closest("#mectrl_main_trigger");

    if (
      !signInButton &&
      !initialsButton &&
      !bingSignInButton &&
      !officeSignInUI
    ) {
      return;
    }

    console.warn(
      "Calling the Storage Access API on behalf of " + STORAGE_ACCESS_ORIGIN
    );

    e.stopPropagation();
    e.preventDefault();
    document.requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN).then(() => {
      target.click();
    });
  },
  true
);
