/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable @microsoft/sdl/no-insecure-url */
"use strict";

// Here we test that HTTPS-First only tries to upgrade known TLDs. In detail:
// httpsfirst.com   -> Should try to upgrade, as .com isn a known TLD
// httpsfirst.local -> Should not try to ipgrade, as .local isn't a known TLD
// We do that by visiting URLs that are only available via HTTP and detect if a
// up- and downgrade happened through the existing Glean temetry.
// Also see Bug 1896083 for reference.

async function runTest(aURL, aExpectUpDowngrade) {
  const initialDowngradeCount = Glean.httpsfirst.downgraded.testGetValue();
  BrowserTestUtils.startLoadingURIString(gBrowser, aURL);
  await BrowserTestUtils.browserLoaded(gBrowser, false, null, true);
  is(
    Glean.httpsfirst.downgraded.testGetValue(),
    aExpectUpDowngrade ? initialDowngradeCount + 1 : initialDowngradeCount,
    `${
      aExpectUpDowngrade ? "A" : "No"
    } up- and downgrade should have happened on ${aURL}`
  );
}

add_task(async function test_tlds() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  await runTest("http://httpsfirst.com", true);
  await runTest("http://httpsfirst.local", false);
  await runTest("http://httpsfirst", false);
});
