/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const TEST_HOST = "example.com";

// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const TEST_URL = "http://" + TEST_HOST;

const TEST_URL_FULL =
  TEST_URL + "/browser/browser/components/contextualidentity/test/browser/";

async function openTabMenuFor(tab) {
  let tabMenu = tab.ownerDocument.getElementById("tabContextMenu");

  let tabMenuShown = BrowserTestUtils.waitForEvent(tabMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    tab,
    { type: "contextmenu" },
    tab.ownerGlobal
  );
  await tabMenuShown;

  return tabMenu;
}

async function openReopenMenuForTab(tab) {
  await openTabMenuFor(tab);

  let reopenItem = tab.ownerDocument.getElementById(
    "context_reopenInContainer"
  );
  ok(!reopenItem.hidden, "Reopen in Container item should be shown");

  let reopenMenu = reopenItem.getElementsByTagName("menupopup")[0];
  let reopenMenuShown = BrowserTestUtils.waitForEvent(reopenMenu, "popupshown");
  reopenItem.openMenu(true);
  await reopenMenuShown;

  return reopenMenu;
}

function openTabInContainer(gBrowser, reopenMenu, id) {
  let tabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_URL_FULL,
    true
  );
  let menuitem = reopenMenu.querySelector(
    `menuitem[data-usercontextid="${id}"]`
  );
  reopenMenu.activateItem(menuitem);
  return tabPromise;
}

add_task(async function testTelemetry() {
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();

  const userContextId = 1;

  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });

  const tab = BrowserTestUtils.addTab(gBrowser, TEST_URL_FULL, {
    userContextId,
  });
  await BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab));

  let tabOpenedEvent = Glean.containers.containerTabOpened.testGetValue();
  Assert.ok(Array.isArray(tabOpenedEvent), "container_tab_opened must exist");
  Assert.equal(tabOpenedEvent.length, 1);
  Assert.equal(tabOpenedEvent[0].extra.container_id, 1);

  const reopenMenu = await openReopenMenuForTab(tab);
  const tab2 = await openTabInContainer(gBrowser, reopenMenu, "2");

  tabOpenedEvent = Glean.containers.containerTabOpened.testGetValue();
  Assert.ok(Array.isArray(tabOpenedEvent), "container_tab_opened must exist");
  Assert.equal(tabOpenedEvent.length, 2);
  Assert.equal(tabOpenedEvent[0].extra.container_id, 1);
  Assert.equal(tabOpenedEvent[1].extra.container_id, 2);

  const tabAssignedEvent = Glean.containers.tabAssignedContainer.testGetValue();
  Assert.ok(Array.isArray(tabAssignedEvent), "tab_assigned_container exists");
  Assert.equal(tabAssignedEvent.length, 1);
  Assert.equal(tabAssignedEvent[0].extra.from_container_id, 1);
  Assert.equal(tabAssignedEvent[0].extra.to_container_id, 2);

  BrowserTestUtils.removeTab(tab);

  let tabClosedEvent = Glean.containers.containerTabClosed.testGetValue();
  Assert.ok(Array.isArray(tabClosedEvent), "container_tab_closed must exist");
  Assert.equal(tabClosedEvent.length, 1);
  Assert.equal(tabClosedEvent[0].extra.container_id, 1);

  BrowserTestUtils.removeTab(tab2);

  tabClosedEvent = Glean.containers.containerTabClosed.testGetValue();
  Assert.ok(Array.isArray(tabClosedEvent), "container_tab_closed must exist");
  Assert.equal(tabClosedEvent.length, 2);
  Assert.equal(tabClosedEvent[0].extra.container_id, 1);
  Assert.equal(tabClosedEvent[1].extra.container_id, 2);
});
