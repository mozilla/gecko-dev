/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test QuickActions.
 */

"use strict";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.secondaryActions.featureGate", true],
    ],
  });
});

const LOAD_TYPE = {
  CURRENT_TAB: 1,
  NEW_TAB: 2,
  PRE_LOADED: 3,
};

let COMMANDS_TESTS = [
  {
    cmd: "open view",
    uri: "about:firefoxview",
    loadType: LOAD_TYPE.PRE_LOADED,
    testFun: async () => {
      await BrowserTestUtils.waitForCondition(() => {
        return (
          window.gBrowser.selectedBrowser.currentURI.spec == "about:firefoxview"
        );
      });
      return true;
    },
  },
  {
    cmd: "add-ons",
    uri: "about:addons",
    testFun: async () => isSelected("button[name=discover]"),
  },
  {
    cmd: "extensions",
    uri: "about:addons",
    numTabPress: 2,
    testFun: async () => isSelected("button[name=extension]"),
  },
  {
    cmd: "themes",
    uri: "about:addons",
    numTabPress: 2,
    testFun: async () => isSelected("button[name=theme]"),
  },
  {
    cmd: "add-ons",
    setup: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "https://example.com/"
      );
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "https://example.com/"
      );
      await onLoad;
    },
    uri: "about:addons",
    loadType: LOAD_TYPE.NEW_TAB,
    testFun: async () => isSelected("button[name=discover]"),
  },
  {
    cmd: "extensions",
    setup: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "https://example.com/"
      );
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "https://example.com/"
      );
      await onLoad;
    },
    uri: "about:addons",
    loadType: LOAD_TYPE.NEW_TAB,
    testFun: async () => isSelected("button[name=extension]"),
    numTabPress: 2,
  },
  {
    cmd: "themes",
    setup: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "https://example.com/"
      );
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "https://example.com/"
      );
      await onLoad;
    },
    uri: "about:addons",
    loadType: LOAD_TYPE.NEW_TAB,
    testFun: async () => isSelected("button[name=theme]"),
    numTabPress: 2,
  },
];

let isSelected = async selector =>
  SpecialPowers.spawn(gBrowser.selectedBrowser, [selector], arg => {
    return ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(arg)?.hasAttribute("selected")
    );
  });

add_task(async function test_pages() {
  for (const {
    cmd,
    uri,
    setup,
    loadType,
    testFun,
    numTabPress = 1,
  } of COMMANDS_TESTS) {
    info(`Testing ${cmd} command is triggered`);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

    if (setup) {
      info("Setup");
      await setup();
    }

    let onLoad =
      loadType == LOAD_TYPE.NEW_TAB
        ? BrowserTestUtils.waitForNewTab(gBrowser, uri, true)
        : BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: cmd,
    });
    for (let i = 0; i < numTabPress; i++) {
      EventUtils.synthesizeKey("KEY_Tab", {}, window);
    }
    EventUtils.synthesizeKey("KEY_Enter", {}, window);

    const newTab =
      loadType == LOAD_TYPE.PRE_LOADED ? gBrowser.selectedTab : await onLoad;

    Assert.ok(
      await testFun(),
      `The command "${cmd}" passed completed its test`
    );

    if ([LOAD_TYPE.NEW_TAB, LOAD_TYPE.PRE_LOADED].includes(loadType)) {
      await BrowserTestUtils.removeTab(newTab);
    }
    await BrowserTestUtils.removeTab(tab);
  }
});
