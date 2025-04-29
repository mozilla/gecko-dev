"use strict";

const TEST_PATH =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  ) + "file_clobbered_property.html";

add_task(async function test_clobbered_properties() {
  Services.fog.testResetFOG();

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PATH);
  BrowserTestUtils.removeTab(tab);

  let result = await TestUtils.waitForCondition(() =>
    Glean.security.shadowedHtmlDocumentPropertyAccess.testGetValue()
  );
  is(result.length, 1, "Got one metric");
  is(
    result[0].extra.name,
    "currentScript",
    "Clobbering of currentScript was collected"
  );
});
