/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function onUpdated_when_grouping_and_ungrouping() {
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      const changes = [];
      browser.tabs.onUpdated.addListener(
        (tabId, changeInfo, tab) => {
          browser.test.assertEq(
            changeInfo.groupId,
            tab.groupId,
            "changeInfo.groupId matches tab.groupId"
          );
          changes.push(changeInfo);
        },
        { properties: ["groupId"] }
      );

      const { id: tabId } = await browser.tabs.create({});
      const groupId1 = await browser.tabs.group({ tabIds: [tabId] });
      await browser.tabs.ungroup(tabId);
      const groupId2 = await browser.tabs.group({ tabIds: [tabId] });
      await browser.tabs.remove(tabId);

      browser.test.assertDeepEq(
        [{ groupId: groupId1 }, { groupId: -1 }, { groupId: groupId2 }],
        changes,
        "Observed tabs.onUpdated events after group(), ungroup() and group()"
      );

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function onUpdated_when_grouping_and_regrouping() {
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      const changes = [];
      browser.tabs.onUpdated.addListener(
        (tabId, changeInfo, tab) => {
          browser.test.assertEq(
            changeInfo.groupId,
            tab.groupId,
            "changeInfo.groupId matches tab.groupId"
          );
          changes.push(changeInfo);
        },
        { properties: ["groupId"] }
      );

      const { id: tabId } = await browser.tabs.create({});
      const groupId1 = await browser.tabs.group({ tabIds: [tabId] });
      const groupId2 = await browser.tabs.group({ tabIds: [tabId] });
      const groupId3 = await browser.tabs.group({ tabIds: [tabId] });
      await browser.tabs.remove(tabId);

      browser.test.assertDeepEq(
        [{ groupId: groupId1 }, { groupId: groupId2 }, { groupId: groupId3 }],
        changes,
        "Observed tabs.onUpdated events after group() and regrouping repeatedly"
      );

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function onUpdated_when_grouping_pinned_tab() {
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      const changes = [];
      browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
        if (changeInfo.groupId) {
          browser.test.assertEq(
            changeInfo.groupId,
            tab.groupId,
            "changeInfo.groupId matches tab.groupId"
          );
          changes.push(changeInfo);
        } else if (changeInfo.pinned != null) {
          changes.push(changeInfo);
        }
      });

      const { id: tabId } = await browser.tabs.create({ pinned: true });
      const groupId = await browser.tabs.group({ tabIds: [tabId] });
      await browser.tabs.remove(tabId);

      browser.test.assertDeepEq(
        [{ pinned: true }, { pinned: false }, { groupId: groupId }],
        changes,
        "Observed tabs.onUpdated events after group() of pinned tab"
      );
      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
