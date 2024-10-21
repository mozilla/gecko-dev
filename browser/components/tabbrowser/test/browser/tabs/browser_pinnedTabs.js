add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", false],
    ],
  })
);
registerCleanupFunction(() => SpecialPowers.popPrefEnv());
var tabs;
var tabbrowser;

function index(tab) {
  return Array.prototype.indexOf.call(tabbrowser.tabs, tab);
}

function indexTest(tab, expectedIndex, msg) {
  var diag = "tab " + tab + " should be at index " + expectedIndex;
  if (msg) {
    msg = msg + " (" + diag + ")";
  } else {
    msg = diag;
  }
  is(index(tabs[tab]), expectedIndex, msg);
}

function PinUnpinHandler(tab, eventName) {
  this.eventCount = 0;
  var self = this;
  tab.addEventListener(
    eventName,
    function () {
      self.eventCount++;
    },
    { capture: true, once: true }
  );
  tabbrowser.tabContainer.addEventListener(
    eventName,
    function (e) {
      if (e.originalTarget == tab) {
        self.eventCount++;
      }
    },
    { capture: true, once: true }
  );
}

add_task(async function test_pinned_horizontal_tabs() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  tabbrowser = win.gBrowser;

  tabs = [
    tabbrowser.selectedTab,
    BrowserTestUtils.addTab(tabbrowser, "about:blank"),
    BrowserTestUtils.addTab(tabbrowser, "about:mozilla"),
    BrowserTestUtils.addTab(tabbrowser, "about:home"),
  ];
  indexTest(0, 0);
  indexTest(1, 1);
  indexTest(2, 2);
  indexTest(3, 3);

  // Discard one of the test tabs to verify that pinning/unpinning
  // discarded tabs does not regress (regression test for Bug 1852391).
  tabbrowser.discardBrowser(tabs[1], true);

  var eh = new PinUnpinHandler(tabs[3], "TabPinned");
  tabbrowser.pinTab(tabs[3]);
  is(eh.eventCount, 2, "TabPinned event should be fired");
  indexTest(0, 1);
  indexTest(1, 2);
  indexTest(2, 3);
  indexTest(3, 0);

  eh = new PinUnpinHandler(tabs[1], "TabPinned");
  tabbrowser.pinTab(tabs[1]);
  is(eh.eventCount, 2, "TabPinned event should be fired");
  indexTest(0, 2);
  indexTest(1, 1);
  indexTest(2, 3);
  indexTest(3, 0);

  tabbrowser.moveTabTo(tabs[3], 3);
  indexTest(3, 1, "shouldn't be able to mix a pinned tab into normal tabs");

  tabbrowser.moveTabTo(tabs[2], 0);
  indexTest(2, 2, "shouldn't be able to mix a normal tab into pinned tabs");

  eh = new PinUnpinHandler(tabs[1], "TabUnpinned");
  tabbrowser.unpinTab(tabs[1]);
  is(eh.eventCount, 2, "TabUnpinned event should be fired");
  indexTest(
    1,
    1,
    "unpinning a tab should move a tab to the start of normal tabs"
  );

  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  let tabStrip = tabbrowser.tabContainer;
  let verticalTabs = document.querySelector("#vertical-tabs");
  let verticalPinnedTabsContainer = document.querySelector(
    "#vertical-pinned-tabs-container"
  );

  is(tabbrowser.pinnedTabCount, 1, "One tab is pinned in horizontal tabstrip");
  ok(tabs[3].pinned, "Third tab is pinned");

  // flip the pref to move the tabstrip into the sidebar
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", true]] });

  ok(BrowserTestUtils.isVisible(verticalTabs), "Vertical tabs slot is visible");
  is(
    tabStrip.parentNode,
    verticalTabs,
    "Tabstrip is slotted into the sidebar vertical tabs container"
  );

  ok(
    BrowserTestUtils.isVisible(verticalPinnedTabsContainer),
    "Vertical pinned tabs container is visible"
  );
  is(
    verticalPinnedTabsContainer.children.length,
    1,
    "One tab is pinned in vertical pinned tabs container"
  );
  is(tabbrowser.pinnedTabCount, 1, "One tab is pinned in global tabstrip");

  tabbrowser.unpinTab(tabs[3]);
  is(tabbrowser.pinnedTabCount, 0, "No tabs are pinned in the global tabstrip");
  is(
    verticalPinnedTabsContainer.children.length,
    0,
    "No tabs are pinned in vertical pinned tabs container"
  );

  tabbrowser.pinTab(tabs[3]);
  tabbrowser.pinTab(tabs[1]);

  is(
    verticalPinnedTabsContainer.children.length,
    2,
    "Two tabs are pinned in the vertical pinned tabs container"
  );

  is(tabbrowser.pinnedTabCount, 2, "Two tabs are pinned in global tabstrip");
  indexTest(
    1,
    1,
    "unpinning a tab should move a tab to the start of normal tabs"
  );

  indexTest(3, 0, "about:home is the first pinned tab");
  indexTest(1, 1, "about:blank is the second pinned tab");

  await BrowserTestUtils.switchTab(tabbrowser, tabs[1]);
  is(tabbrowser.selectedTab, tabs[1], "about:blank is the selected tab");

  tabbrowser.moveTabToStart();
  indexTest(1, 0, "about:blank is now the first pinned tab");
  indexTest(3, 1, "about:home is now the second pinned tab");
  is(
    verticalPinnedTabsContainer.children[0],
    tabs[1],
    "about:blank is the first tab in the pinned tabs container"
  );

  tabbrowser.pinTab(tabs[2]);
  indexTest(1, 0, "about:blank is now the first pinned tab");
  indexTest(2, 2, "about:mozilla is now the third pinned tab");
  indexTest(3, 1, "about:home is now the second pinned tab");

  await BrowserTestUtils.switchTab(tabbrowser, tabs[3]);
  is(tabbrowser.selectedTab, tabs[3], "about:home is the selected tab");

  tabbrowser.moveTabToEnd();
  indexTest(1, 0, "about:blank is now the first pinned tab");
  indexTest(2, 1, "about:mozilla is now the second pinned tab");
  indexTest(3, 2, "about:home is now the third pinned tab");

  tabbrowser.moveTabTo(tabs[1], 1);
  indexTest(1, 1, "about:blank is now the second pinned tab");
  indexTest(2, 0, "about:mozilla is now the first pinned tab");
  indexTest(3, 2, "about:home is now the third pinned tab");

  is(
    verticalPinnedTabsContainer.children[2],
    tabs[3],
    "about:home is the last tab in the pinned tabs container"
  );

  // flip the pref to move the tabstrip back into original location
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", false]] });

  await TestUtils.waitForCondition(
    () => !verticalPinnedTabsContainer.children.length,
    "Pinned tabs are no longer in vertical pinned tabs container"
  );
  is(
    tabbrowser.pinnedTabCount,
    3,
    "One tab is still pinned in global tabstrip"
  );
  indexTest(1, 1, "about:blank is still the second pinned tab");
  indexTest(2, 0, "about:mozilla is still the first pinned tab");
  indexTest(3, 2, "about:home is still the third pinned tab");
  indexTest(0, 3, "initial tab is still the last tab");

  await BrowserTestUtils.closeWindow(win);
});
