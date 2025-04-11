/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function resetInternalExtensionFallbackTabGroupIdMap() {
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs"
  );
  // ExtensionParent.apiManager.global may be unset if there has not been any
  // attempt to load any extension yet.
  // We need to use eval() because the variables are declared with const/let,
  // which cannot simply be read as properties from the global.
  ExtensionParent.apiManager.global?.eval(`
    "use strict";
    // Reset to initial values as set by ext-browser.js
    fallbackTabGroupIdMap.clear();
    nextFallbackTabGroupId = 1;
  `);
}
function getInternalExtensionFallbackTabGroupIdMapEntries() {
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs"
  );
  return Array.from(
    ExtensionParent.apiManager.global.eval("fallbackTabGroupIdMap").entries()
  );
}

async function getCurrentTabGroupId() {
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      const [tab] = await browser.tabs.query({
        active: true,
        lastFocusedWindow: true,
      });
      browser.test.assertTrue(
        Number.isSafeInteger(tab.groupId),
        `groupId is safe integer: ${tab.groupId}`
      );
      browser.test.assertTrue(
        tab.groupId >= 0,
        `groupId is non-negative integer: ${tab.groupId}`
      );
      const tabRepeated = await browser.tabs.get(tab.id);
      browser.test.assertEq(
        tab.groupId,
        tabRepeated.groupId,
        "groupId should consistently return the same value"
      );
      const tabs = await browser.tabs.query({ groupId: tab.groupId });
      browser.test.assertEq(
        tabs.length,
        1,
        "tabs.query({ groupId }) found the one and only tab in that group"
      );
      browser.test.assertEq(tab.id, tabs[0].id, "Got expected tab");
      browser.test.sendMessage("ext_groupId", tab.groupId);
    },
  });
  await extension.startup();
  const groupId = await extension.awaitMessage("ext_groupId");
  await extension.unload();
  return groupId;
}

// This test checks whether the tabs.Tab.groupId field has a meaningful value
// derived from a tabbrowser tab group.
add_task(async function tabs_Tab_groupId() {
  resetInternalExtensionFallbackTabGroupIdMap();

  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/?1",
    true
  );
  const tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/?2",
    true
  );

  gBrowser.selectedTab = tab2;

  // Without a given ID, tabbrowser generates a new ID.
  const group1 = gBrowser.addTabGroup([tab1]);
  // The following ID differs from the tabbrowser format:
  const group2 = gBrowser.addTabGroup([tab2], { id: "non-numericid" });
  is(group2.id, "non-numericid", "Created group with non-numeric ID");

  info("Testing tab group with internally generated ID");
  gBrowser.selectedTab = tab1;
  const generatedId1 = await getCurrentTabGroupId();
  const generatedId2 = await getCurrentTabGroupId();
  is(generatedId1, generatedId2, "Same groupId across extensions (generated)");

  info("Testing tab group with ID, not matching internally generated IDs");
  gBrowser.selectedTab = tab2;
  const strangeId1 = await getCurrentTabGroupId();
  const strangeId2 = await getCurrentTabGroupId();
  is(strangeId1, strangeId2, "Same groupId across extensions (non-internal)");

  isnot(generatedId1, strangeId1, "Independent tab groups have different ID");

  // Move tab to another tab group.
  const group3 = gBrowser.addTabGroup([tab2], { id: "another-non-numericid" });
  is(group3.id, "another-non-numericid", "Updated group with non-numeric ID");
  const strangeId3 = await getCurrentTabGroupId();
  isnot(strangeId2, strangeId3, "New tab group has different tab group ID");

  // Verify assumptions behind the above tests, to make sure that the test
  // passes for the expected reasons.
  is(
    `${Math.floor(generatedId1 / 1000)}-${generatedId1 % 1000}`,
    group1.id,
    "groupId in extension API derived from internally-generated group ID"
  );
  is(
    strangeId1,
    1,
    "groupId in extension API is next available ID (non-numericid)"
  );
  is(
    strangeId3,
    2,
    "groupId in extension API is next available ID (another-non-numericid)"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  // The main purpose of this check is to verify that we do not store mappings
  // for internal groupIds that are in the expected format. Additionally, this
  // also shows that the map is not cleaned up even after the group disappears.
  Assert.deepEqual(
    getInternalExtensionFallbackTabGroupIdMapEntries(),
    [
      ["non-numericid", strangeId1],
      ["another-non-numericid", strangeId3],
    ],
    "The internal fallback map contains only non-numeric IDs"
  );

  resetInternalExtensionFallbackTabGroupIdMap();
});
