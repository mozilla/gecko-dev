/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
  SyncedTabsErrorHandler:
    "resource:///modules/firefox-view-synced-tabs-error-handler.sys.mjs",
  TabsSetupFlowManager:
    "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs",
});

const tabClients = [
  {
    id: 1,
    type: "client",
    name: "My desktop",
    clientType: "desktop",
    lastModified: 1655730486760,
    tabs: [
      {
        device: "My desktop",
        deviceType: "desktop",
        type: "tab",
        title: "Sandboxes - Sinon.JS",
        url: "https://sinonjs.org/releases/latest/sandbox/",
        icon: "https://sinonjs.org/assets/images/favicon.png",
        lastUsed: 1655391592, // Thu Jun 16 2022 14:59:52 GMT+0000
        client: 1,
      },
      {
        device: "My desktop",
        deviceType: "desktop",
        type: "tab",
        title: "Internet for people, not profits - Mozilla",
        url: "https://www.mozilla.org/",
        icon: "https://www.mozilla.org/media/img/favicons/mozilla/favicon.d25d81d39065.ico",
        lastUsed: 1655730486, // Mon Jun 20 2022 13:08:06 GMT+0000
        client: 1,
      },
    ],
  },
  {
    id: 2,
    type: "client",
    name: "My iphone",
    clientType: "phone",
    lastModified: 1655727832930,
    tabs: [
      {
        device: "My iphone",
        deviceType: "mobile",
        type: "tab",
        title: "The Guardian",
        url: "https://www.theguardian.com/",
        icon: "page-icon:https://www.theguardian.com/",
        lastUsed: 1655291890, // Wed Jun 15 2022 11:18:10 GMT+0000
        client: 2,
      },
      {
        device: "My iphone",
        deviceType: "mobile",
        type: "tab",
        title: "The Times",
        url: "https://www.thetimes.co.uk/",
        icon: "page-icon:https://www.thetimes.co.uk/",
        lastUsed: 1655727485, // Mon Jun 20 2022 12:18:05 GMT+0000
        client: 2,
      },
    ],
  },
];

add_task(async function test_tabs() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(lazy.SyncedTabsErrorHandler, "getErrorType").returns(null);
  sandbox.stub(lazy.TabsSetupFlowManager, "uiStateIndex").value(4);
  sandbox.stub(lazy.SyncedTabs, "getTabClients").resolves(tabClients);
  sandbox
    .stub(lazy.SyncedTabs, "createRecentTabsList")
    .resolves(tabClients.flatMap(client => client.tabs));

  await SidebarController.show("viewTabsSidebar");
  const { contentDocument } = SidebarController.browser;
  const component = contentDocument.querySelector("sidebar-syncedtabs");
  Assert.ok(component, "Synced tabs panel is shown.");
  const contextMenu = SidebarController.currentContextMenu;

  for (const [i, client] of tabClients.entries()) {
    const card = component.cards[i];
    Assert.equal(card.heading, client.name, "Device name is correct.");
    const rows = await TestUtils.waitForCondition(() => {
      const { rowEls } = card.querySelector("fxview-tab-list");
      return rowEls.length === client.tabs.length && rowEls;
    }, "Device has the correct number of tabs.");
    for (const [j, row] of rows.entries()) {
      const tabData = client.tabs[j];
      Assert.equal(row.title, tabData.title, `Tab ${j + 1} has correct title.`);
      Assert.equal(row.url, tabData.url, `Tab ${j + 1} has correct URL.`);
    }
  }

  info("Copy the first link.");
  const tabList = component.cards[0].querySelector("fxview-tab-list");
  const menuItem = document.getElementById(
    "sidebar-synced-tabs-context-copy-link"
  );
  await openAndWaitForContextMenu(contextMenu, tabList.rowEls[0].mainEl, () =>
    contextMenu.activateItem(menuItem)
  );
  await TestUtils.waitForCondition(() => {
    const copiedUrl = SpecialPowers.getClipboardData("text/plain");
    return copiedUrl == tabClients[0].tabs[0].url;
  }, "The copied URL is correct.");

  SidebarController.hide();
  sandbox.restore();
});
