/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_tabGroups() {
  let group = gBrowser.addTabGroup("blue", "test");

  Assert.ok(group.id, "group has id");

  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tab1]);

  Assert.ok(group.tabs.includes(tab1), "tab1 is in group");

  // TODO add API to remove group
  BrowserTestUtils.removeTab(tab1);
  group.remove();
});
