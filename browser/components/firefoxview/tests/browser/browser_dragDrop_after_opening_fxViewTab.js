/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that dragging and dropping tabs into tabbrowser works as intended
 * after opening the Firefox View tab for RTL builds. There was an issue where
 * tabs from dragged links were not dropped in the correct tab indexes
 * for RTL builds because logic for RTL builds did not take into consideration
 * hidden tabs like the Firefox View tab. This test makes sure that this behavior does not reoccur.
 */
add_task(async function () {
  info("Setting browser to RTL locale");
  await BrowserTestUtils.enableRtlLocale();

  const TEST_ROOT = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  );
  let newTab = gBrowser.tabs[0];

  let waitForTestTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_ROOT + "file_new_tab_page.html"
  );
  let testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_ROOT + "file_new_tab_page.html"
  );
  await waitForTestTabPromise;

  // FIXME (bug 1958909): This uses the wrong document.
  let linkSrcEl = document.querySelector("a");
  ok(linkSrcEl, "Link exists");

  let dropPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "drop"
  );

  /**
   * There should be 2 tabs:
   * 1. new tab (auto-generated)
   * 2. test tab
   */
  is(gBrowser.visibleTabs.length, 2, "There should be 2 tabs");

  // Now open Firefox View tab
  info("Opening Firefox View tab");
  await openFirefoxViewTab(window);

  /**
   * There should be 2 visible tabs:
   * 1. new tab (auto-generated)
   * 2. test tab
   * Firefox View tab is hidden.
   */
  is(
    gBrowser.visibleTabs.length,
    2,
    "There should still be 2 visible tabs after opening Firefox View tab"
  );

  info("Switching to test tab");
  await BrowserTestUtils.switchTab(gBrowser, testTab);

  let waitForDraggedTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/#test"
  );

  info("Dragging link between test tab and new tab");
  EventUtils.synthesizeDrop(
    linkSrcEl,
    testTab,
    [[{ type: "text/plain", data: "https://example.com/#test" }]],
    "link",
    window,
    window,
    {
      clientX: testTab.getBoundingClientRect().right,
    }
  );

  info("Waiting for drop event");
  await dropPromise;
  info("Waiting for dragged tab to be created");
  let draggedTab = await waitForDraggedTabPromise;

  /**
   * There should be 3 visible tabs:
   * 1. new tab (auto-generated)
   * 2. new tab from dragged link
   * 3. test tab
   *
   * In RTL build, it should appear in the following order:
   * <test tab> <link dragged tab> <new tab> | <FxView tab>
   */
  is(gBrowser.visibleTabs.length, 3, "There should be 3 tabs");
  is(
    gBrowser.visibleTabs.indexOf(newTab),
    0,
    "New tab should still be rightmost visible tab"
  );
  is(
    gBrowser.visibleTabs.indexOf(draggedTab),
    1,
    "Dragged link should positioned at new index"
  );
  is(
    gBrowser.visibleTabs.indexOf(testTab),
    2,
    "Test tab should be to the left of dragged tab"
  );
  BrowserTestUtils.removeTab(draggedTab);
  BrowserTestUtils.removeTab(testTab);
  BrowserTestUtils.removeTab(FirefoxViewHandler.tab);

  await BrowserTestUtils.disableRtlLocale();
});
