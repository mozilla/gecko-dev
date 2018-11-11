/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const remoteClientsFixture = [ { id: 1, name: "Foo"}, { id: 2, name: "Bar"} ];

add_task(function* test() {
  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;
  is(gBrowser.visibleTabs.length, 1, "there is one visible tab");
  let testTab = gBrowser.addTab();
  is(gBrowser.visibleTabs.length, 2, "there are now two visible tabs");

  // Check the context menu with two tabs
  updateTabContextMenu(origTab);
  is(document.getElementById("context_closeTab").disabled, false, "Close Tab is enabled");
  is(document.getElementById("context_reloadAllTabs").disabled, false, "Reload All Tabs is enabled");


  if (gFxAccounts.sendTabToDeviceEnabled) {
    // Check the send tab to device menu item
    const oldGetter = setupRemoteClientsFixture(remoteClientsFixture);
    yield updateTabContextMenu(origTab, function* () {
      yield openMenuItemSubmenu("context_sendTabToDevice");
    });
    is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
    let targets = document.getElementById("context_sendTabToDevicePopupMenu").childNodes;
    is(targets[0].getAttribute("label"), "Foo", "Foo target is present");
    is(targets[1].getAttribute("label"), "Bar", "Bar target is present");
    is(targets[3].getAttribute("label"), "All Devices", "All Devices target is present");
    restoreRemoteClients(oldGetter);
  }

  // Hide the original tab.
  gBrowser.selectedTab = testTab;
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.visibleTabs.length, 1, "now there is only one visible tab");

  // Check the context menu with one tab.
  updateTabContextMenu(testTab);
  is(document.getElementById("context_closeTab").disabled, false, "Close Tab is enabled when more than one tab exists");
  is(document.getElementById("context_reloadAllTabs").disabled, true, "Reload All Tabs is disabled");

  // Add a tab that will get pinned
  // So now there's one pinned tab, one visible unpinned tab, and one hidden tab
  let pinned = gBrowser.addTab();
  gBrowser.pinTab(pinned);
  is(gBrowser.visibleTabs.length, 2, "now there are two visible tabs");

  // Check the context menu on the unpinned visible tab
  updateTabContextMenu(testTab);
  is(document.getElementById("context_closeOtherTabs").disabled, true, "Close Other Tabs is disabled");
  is(document.getElementById("context_closeTabsToTheEnd").disabled, true, "Close Tabs To The End is disabled");

  // Show all tabs
  let allTabs = Array.from(gBrowser.tabs);
  gBrowser.showOnlyTheseTabs(allTabs);

  // Check the context menu now
  updateTabContextMenu(testTab);
  is(document.getElementById("context_closeOtherTabs").disabled, false, "Close Other Tabs is enabled");
  is(document.getElementById("context_closeTabsToTheEnd").disabled, true, "Close Tabs To The End is disabled");

  // Check the context menu of the original tab
  // Close Tabs To The End should now be enabled
  updateTabContextMenu(origTab);
  is(document.getElementById("context_closeTabsToTheEnd").disabled, false, "Close Tabs To The End is enabled");

  gBrowser.removeTab(testTab);
  gBrowser.removeTab(pinned);
});

