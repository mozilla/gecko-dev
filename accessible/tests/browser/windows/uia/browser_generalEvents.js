/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the AsyncContentLoaded event.
 */
addUiaTask(``, async function testAsyncContentLoaded(browser) {
  info("Loading new document");
  await setUpWaitForUiaEvent("AsyncContentLoaded", "uiaTestDoc");
  const encoded = encodeURIComponent('<body id="uiaTestDoc">test');
  browser.loadURI(Services.io.newURI(`data:text/html,${encoded}`), {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  await waitForUiaEvent();
  ok(true, "Got AsyncContentLoaded event");
});
