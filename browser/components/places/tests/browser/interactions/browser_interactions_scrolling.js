/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test reporting of scrolling interactions.
 */

"use strict";

const TEST_URL =
  "https://example.com/browser/browser/components/places/tests/browser/interactions/scrolling.html";
const TEST_URL2 = "https://example.com/browser";

async function waitForScrollEvent(aBrowser, aTask) {
  let promise = BrowserTestUtils.waitForContentEvent(aBrowser, "scroll");

  // This forces us to send a message to the browser's process and receive a response which ensures
  // that the message sent to register the scroll event listener will also have been processed by
  // the content process. Without this it is possible for our scroll task to send a higher priority
  // message which can be processed by the content process before the message to register the scroll
  // event listener.
  await SpecialPowers.spawn(aBrowser, [], () => {});

  await aTask();
  await promise;
}

add_task(async function test_no_scrolling() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    BrowserTestUtils.startLoadingURIString(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_arrow_key_down_scroll() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await SpecialPowers.spawn(browser, [], function () {
      const heading = content.document.getElementById("heading");
      heading.focus();
    });

    await waitForScrollEvent(browser, () =>
      EventUtils.synthesizeKey("KEY_ArrowDown")
    );

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        scrollingDistanceIsGreaterThan: 0,
        scrollingTimeIsGreaterThan: 0,
      },
    ]);
  });
});

add_task(async function test_scrollIntoView() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await waitForScrollEvent(browser, () =>
      SpecialPowers.spawn(browser, [], function () {
        const heading = content.document.getElementById("middleHeading");
        heading.scrollIntoView();
      })
    );

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // JS-triggered scrolling should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_anchor_click() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await waitForScrollEvent(browser, () =>
      SpecialPowers.spawn(browser, [], function () {
        const anchor = content.document.getElementById("to_bottom_anchor");
        anchor.click();
      })
    );

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // The scrolling resulting from clicking on an anchor should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_window_scrollBy() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await waitForScrollEvent(browser, () =>
      SpecialPowers.spawn(browser, [], function () {
        content.scrollBy(0, 100);
      })
    );

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // The scrolling resulting from the window.scrollBy() call should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_window_scrollTo() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await waitForScrollEvent(browser, () =>
      SpecialPowers.spawn(browser, [], function () {
        content.scrollTo(0, 200);
      })
    );

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    // The scrolling resulting from the window.scrollTo() call should not be reported
    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactscrollingDistance: 0,
        exactscrollingTime: 0,
      },
    ]);
  });
});

add_task(async function test_window_scroll_switch_tabs() {
  await Interactions.reset();

  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  info("Scroll some distance on first tab");
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], function () {
    const heading = content.document.getElementById("heading");
    heading.focus();
  });
  await waitForScrollEvent(browser, () =>
    EventUtils.synthesizeKey("KEY_ArrowDown")
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL2,
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
  ]);

  info("Switch to first tab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  await assertDatabaseValues([
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL2,
      exactscrollingDistance: 0,
      exactscrollingTime: 0,
    },
  ]);

  info("Scroll some distance on first tab");
  browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], function () {
    const heading = content.document.getElementById("heading");
    heading.focus();
  });
  await waitForScrollEvent(browser, () =>
    EventUtils.synthesizeKey("KEY_ArrowDown")
  );

  info("Switch to second tab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL2,
      exactscrollingDistance: 0,
      exactscrollingTime: 0,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL2,
      exactscrollingDistance: 0,
      exactscrollingTime: 0,
    },
  ]);
});

add_task(async function test_window_scroll_switch_tabs_delayed() {
  await Interactions.reset();

  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  info("Scroll some distance on first tab");
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], function () {
    const heading = content.document.getElementById("heading");
    heading.focus();
  });
  await waitForScrollEvent(browser, () =>
    EventUtils.synthesizeKey("KEY_ArrowDown")
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL2,
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
  ]);

  // Doing actions in a tab can increase the amount of time another tab is not
  // viewed, so to avoid potentially triggering intermittents, adjust the
  // timer close to when we expect to see the result and until the test ends
  // minimize the number of interactions on the page.
  const THRESHOLD_MS = 500;
  setTabSelectIdleTimer(THRESHOLD_MS);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, THRESHOLD_MS));
  info("Switch to first tab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  await assertDatabaseValues([
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL2,
      exactscrollingDistance: 0,
      exactscrollingTime: 0,
    },
  ]);

  info("Scroll some distance on first tab");
  browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], function () {
    const heading = content.document.getElementById("heading");
    heading.focus();
  });
  await waitForScrollEvent(browser, () =>
    EventUtils.synthesizeKey("KEY_ArrowDown")
  );

  info("Switch to second tab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
    {
      url: TEST_URL2,
      exactscrollingDistance: 0,
      exactscrollingTime: 0,
    },
    {
      url: TEST_URL,
      scrollingDistanceIsGreaterThan: 0,
      scrollingTimeIsGreaterThan: 0,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
