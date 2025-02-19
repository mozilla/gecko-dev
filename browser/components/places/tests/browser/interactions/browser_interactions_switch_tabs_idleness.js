/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests when switching tabs and the idle time meets or exceeds the threshold,
 * a new interaction is generated.
 */

const TEST_URL = "https://example.com/";
const TEST_URL2 = "https://example.com/browser";

add_task(async function test_interactions_switch_tabs() {
  await Interactions.reset();

  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL2);

  info("Switch to second tab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
  ]);

  info("Switch to first tab again");
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
    {
      url: TEST_URL2,
    },
  ]);

  info("Switch to second tab again");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
    {
      url: TEST_URL2,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
    {
      url: TEST_URL2,
    },
  ]);
});

add_task(async function test_interactions_switch_tabs_delayed() {
  await Interactions.reset();

  info(
    "Reduce delay threshold so any tab switch results in a new interaction."
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.places.interactions.breakupIfNoUpdatesForSeconds", 0]],
  });

  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL2);

  info("Switch to second tab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
    {
      // This second interaction was generated because its associated tab was
      // opened in the background while the first tab was focused, and we didn't
      // select the tab until after the threshold.
      url: TEST_URL2,
    },
  ]);

  info("Switch to first tab again");
  await BrowserTestUtils.switchTab(gBrowser, tab1);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
    {
      url: TEST_URL2,
    },
    {
      url: TEST_URL2,
    },
  ]);

  info("Switch to second tab again");
  await BrowserTestUtils.switchTab(gBrowser, tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
    {
      url: TEST_URL2,
    },
    {
      url: TEST_URL2,
    },
    {
      url: TEST_URL,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  await assertDatabaseValues([
    {
      url: TEST_URL,
    },
    {
      url: TEST_URL2,
    },
    {
      url: TEST_URL2,
    },
    {
      url: TEST_URL,
    },
    {
      url: TEST_URL2,
    },
  ]);

  await SpecialPowers.popPrefEnv();
});
