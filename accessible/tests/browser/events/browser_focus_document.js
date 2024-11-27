/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that switching from a remote document to a non-remote document fires
 * focus correctly.
 */
addAccessibleTask(``, async function testRemoteThenLocal(browser) {
  info("Loading about:support into same tab");
  let focused = waitForEvent(EVENT_FOCUS, event => {
    const acc = event.accessible;
    acc.QueryInterface(nsIAccessibleDocument);
    return acc.URL == "about:support";
  });
  browser.loadURI(Services.io.newURI("about:support"), {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  await focused;
});
