/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_browsingContextWithNoOpenerHasCrossGroupOpener() {
  const onNewTab = BrowserTestUtils.waitForNewTab(gBrowser, TEST_PATH);
  const openerBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [TEST_PATH],
    async function (testPath) {
      content.open(testPath, "_blank", "noopener");
      return content.browsingContext;
    }
  );
  const newTab = await onNewTab;

  await BrowserTestUtils.browserLoaded(newTab.linkedBrowser, false, TEST_PATH);

  const browsingContext = newTab.linkedBrowser.browsingContext;
  Assert.equal(
    browsingContext.opener,
    null,
    "A tab opened with noopener shouldn't have an opener"
  );
  Assert.ok(
    browsingContext.crossGroupOpener,
    "A cross origin A tab opened with noopener should have a crossGroupOpener"
  );
  Assert.equal(
    browsingContext.crossGroupOpener,
    openerBrowsingContext,
    "The crossGroupOpener should be the same as the actual opener"
  );

  await BrowserTestUtils.removeTab(newTab);
});
