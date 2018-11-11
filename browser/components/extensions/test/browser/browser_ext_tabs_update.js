/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:config");

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: function() {
      browser.tabs.query({
        lastFocusedWindow: true,
      }, function(tabs) {
        browser.test.assertEq(tabs.length, 3, "should have three tabs");

        tabs.sort((tab1, tab2) => tab1.index - tab2.index);

        browser.test.assertEq(tabs[0].url, "about:blank", "first tab blank");
        tabs.shift();

        browser.test.assertTrue(tabs[0].active, "tab 0 active");
        browser.test.assertFalse(tabs[1].active, "tab 1 inactive");

        browser.tabs.update(tabs[1].id, {active: true}, function() {
          browser.test.sendMessage("check");
        });
      });
    },
  });

  yield Promise.all([extension.startup(), extension.awaitMessage("check")]);

  ok(gBrowser.selectedTab == tab2, "correct tab selected");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
