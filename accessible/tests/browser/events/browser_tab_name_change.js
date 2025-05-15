/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(``, async function (browser, _accDoc) {
  let tabChanged = waitForEvent(
    EVENT_FOCUS,
    e => e.accessible.name == "One" && e.accessible.role == ROLE_DOCUMENT
  );

  // Put new tab in foreground
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "data:text/html,<title>One</title>",
  });

  await tabChanged;

  let nameChanged = waitForEvent(
    EVENT_NAME_CHANGE,
    e => e.accessible.name == "Two" && e.accessible.role == ROLE_PAGETAB
  );

  // Change name of background tab
  BrowserTestUtils.startLoadingURIString(
    browser,
    "data:text/html,<title>Two</title>"
  );

  await nameChanged;

  BrowserTestUtils.removeTab(tab);
});
