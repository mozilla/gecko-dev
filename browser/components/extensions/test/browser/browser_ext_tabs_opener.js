/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function () {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank?1"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank?2"
  );

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background() {
      let activeTab;
      let tabId;
      let tabIds;
      browser.tabs
        .query({ lastFocusedWindow: true })
        .then(tabs => {
          browser.test.assertEq(3, tabs.length, "We have three tabs");

          browser.test.assertTrue(tabs[1].active, "Tab 1 is active");
          activeTab = tabs[1];

          tabIds = tabs.map(tab => tab.id);

          return browser.tabs.create({
            openerTabId: activeTab.id,
            active: false,
          });
        })
        .then(tab => {
          browser.test.assertEq(
            activeTab.id,
            tab.openerTabId,
            "Tab opener ID is correct"
          );
          browser.test.assertEq(
            activeTab.index + 1,
            tab.index,
            "Tab was inserted after the related current tab"
          );

          tabId = tab.id;
          return browser.tabs.get(tabId);
        })
        .then(tab => {
          browser.test.assertEq(
            activeTab.id,
            tab.openerTabId,
            "Tab opener ID is still correct"
          );

          return browser.tabs.update(tabId, { openerTabId: tabIds[0] });
        })
        .then(tab => {
          browser.test.assertEq(
            tabIds[0],
            tab.openerTabId,
            "Updated tab opener ID is correct"
          );

          return browser.tabs.get(tabId);
        })
        .then(tab => {
          browser.test.assertEq(
            tabIds[0],
            tab.openerTabId,
            "Updated tab opener ID is still correct"
          );

          return browser.tabs.create({ openerTabId: tabId, active: false });
        })
        .then(tab => {
          browser.test.assertEq(
            tabId,
            tab.openerTabId,
            "New tab opener ID is correct"
          );
          browser.test.assertEq(
            tabIds.length,
            tab.index,
            "New tab was not inserted after the unrelated current tab"
          );

          let promise = browser.tabs.remove(tabId);

          tabId = tab.id;
          return promise;
        })
        .then(() => {
          return browser.tabs.get(tabId);
        })
        .then(tab => {
          browser.test.assertEq(
            undefined,
            tab.openerTabId,
            "Tab opener ID was cleared after opener tab closed"
          );

          return browser.tabs.remove(tabId);
        })
        .then(() => {
          browser.test.notifyPass("tab-opener");
        })
        .catch(e => {
          browser.test.fail(`${e} :: ${e.stack}`);
          browser.test.notifyFail("tab-opener");
        });
    },
  });

  await extension.startup();

  await extension.awaitFinish("tab-opener");

  await extension.unload();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_tabs_onUpdated_fired_on_openerTabId_change() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    async background() {
      let changedOpenerTabIds = [];
      let promise = new Promise(resolve => {
        browser.tabs.onUpdated.addListener((tabId, changeInfo, _tab) => {
          const { openerTabId } = changeInfo;
          if (openerTabId) {
            browser.test.assertDeepEq(
              { openerTabId },
              changeInfo,
              `"openerTabId" is the only key in changeInfo for tab ${tabId}`
            );
            changedOpenerTabIds.push(openerTabId);
            if (openerTabId === -1) {
              // The last part of the test changes openerTabId back to -1.
              resolve();
            }
          }
        });
      });

      try {
        let tab1 = await browser.tabs.create({});
        let tab2 = await browser.tabs.create({});

        browser.test.assertDeepEq(
          [],
          changedOpenerTabIds,
          "No tabs.onUpdated fired with openerTabId at tab creation"
        );

        // Not changed, should not emit event:
        await browser.tabs.update(tab1.id, { openerTabId: -1 });
        // Should emit event:
        await browser.tabs.update(tab1.id, { openerTabId: tab2.id });
        // Not changed, should not emit event:
        await browser.tabs.update(tab1.id, { openerTabId: tab2.id });
        // Should emit event:
        await browser.tabs.update(tab1.id, { openerTabId: -1 });
        await promise;

        browser.test.assertDeepEq(
          [tab2.id, -1],
          changedOpenerTabIds,
          "Got expected tabs.onUpdated for openerTabId changes"
        );

        await browser.tabs.remove(tab1.id);
        await browser.tabs.remove(tab2.id);

        browser.test.notifyPass("tab-onUpdated-opener");
      } catch (e) {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("tab-onUpdated-opener");
      }
    },
  });

  await extension.startup();
  await extension.awaitFinish("tab-onUpdated-opener");
  await extension.unload();
});

add_task(async function test_errors_on_openerTabId_change() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    async background() {
      let tab = await browser.tabs.create({});

      await browser.test.assertRejects(
        browser.tabs.update(tab.id, { openerTabId: 123456789 }),
        "Invalid tab ID: 123456789",
        "Got error when openerTabId is invalid"
      );

      let win = await browser.windows.create({ url: "about:blank" });

      await browser.test.assertRejects(
        browser.tabs.update(tab.id, { openerTabId: win.tabs[0].id }),
        "Opener tab must be in the same window as the tab being updated",
        "Got error when openerTabId belongs to a different window"
      );

      tab = await browser.tabs.get(tab.id);

      browser.test.assertEq(
        undefined,
        tab.openerTabId,
        "Got initial tab.openerTabId after failing updates"
      );

      await browser.windows.remove(win.id);
      await browser.tabs.remove(tab.id);

      browser.test.notifyPass("tab-opener-with-wrong-window");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tab-opener-with-wrong-window");
  await extension.unload();
});
