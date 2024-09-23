/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let listService;

// Tests for the strip on share functionality of the urlbar.

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_list", "stripParam"],
      ["privacy.query_stripping.enabled", false],
      ["privacy.query_stripping.strip_on_share.canDisable", false],
    ],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );

  await listService.testWaitForInit();
});

// Selection is not a valid URI, menu item should be hidden
add_task(async function testInvalidURI() {
  await testMenuItemDisabled(
    "https://www.example.com/?stripParam=1234",
    true,
    false,
    true
  );
});

// Pref is not enabled, menu item should be hidden
add_task(async function testPrefDisabled() {
  await testMenuItemDisabled(
    "https://www.example.com/?stripParam=1234",
    false,
    false,
    false
  );
});

// Menu item should be visible, the whole url is copied without a selection, url should be stripped.
add_task(async function testQueryParamIsStripped() {
  let originalUrl = "https://www.example.com/?stripParam=1234";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: false,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: false,
    canDisable: false,
    isDisabled: false,
  });
});

// Menu item should be visible, selecting the whole url, url should be stripped.
add_task(async function testQueryParamIsStrippedSelectURL() {
  let originalUrl = "https://www.example.com/?stripParam=1234";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: false,
    canDisable: false,
    isDisabled: false,
  });
});

// Menu item should be visible even with the disable pref enabled
// selecting the whole url, url should be stripped.
add_task(async function testQueryParamIsStrippedWithDisabletPref() {
  let originalUrl = "https://www.example.com/?stripParam=1234";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: false,
    canDisable: true,
    isDisabled: false,
  });
});

// Make sure other parameters don't interfere with stripping
add_task(async function testQueryParamIsStrippedWithDisabletPref() {
  let originalUrl = "https://www.example.com/?keepParameter=1&stripParam=1234";
  let shortenedUrl = "https://www.example.com/?keepParameter=1";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: false,
    canDisable: true,
    isDisabled: false,
  });
});

// Test that menu item becomes visible again after selecting a non-url
add_task(async function testQueryParamIsStrippedWithDisabletPref() {
  // Selection is not a valid URI, menu item should be hidden
  await testMenuItemDisabled(
    "https://www.example.com/?stripParam=1234",
    true,
    true,
    true
  );
  // test if menu item is visible after it getting hidden
  let originalUrl = "https://www.example.com/?stripParam=1234";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: false,
    canDisable: true,
    isDisabled: false,
  });
});

// Menu item should be visible, selecting the whole url, url should be the same.
add_task(async function testURLIsCopiedWithNoParams() {
  let originalUrl = "https://www.example.com/";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: false,
    canDisable: false,
    isDisabled: false,
  });
});

// Menu item should be visible, selecting the whole url, url should be the same.
add_task(async function testURLIsCopiedWithNoParams() {
  let originalUrl = "https://www.example.com/";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: false,
    canDisable: true,
    isDisabled: true,
  });
});

// Testing site specific parameter stripping
add_task(async function testQueryParamIsStrippedForSiteSpecific() {
  let originalUrl = "https://www.example.com/?test_2=1234";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: true,
    canDisable: false,
    isDisabled: false,
  });
});

// Ensuring site specific parameters are not stripped for other sites
add_task(async function testQueryParamIsNotStrippedForWrongSiteSpecific() {
  let originalUrl = "https://www.example.com/?test_3=1234";
  let shortenedUrl = "https://www.example.com/?test_3=1234";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: true,
    canDisable: false,
    isDisabled: false,
  });
});

// Ensuring site specific parameters are stripped regardless
// of capitalization in the URI
add_task(async function testQueryParamIsStrippedWhenParamIsCapitalized() {
  let originalUrl = "https://www.example.com/?TEST_1=1234";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: true,
    canDisable: false,
    isDisabled: false,
  });
});

// Ensuring site specific parameters are stripped regardless
// of capitalization in the URI
add_task(async function testQueryParamIsStrippedWhenParamIsCapitalized() {
  let originalUrl = "https://www.example.com/?test_5=1234";
  let shortenedUrl = "https://www.example.com/";
  await testMenuItemEnabled({
    selectWholeUrl: true,
    validUrl: originalUrl,
    strippedUrl: shortenedUrl,
    useTestList: true,
    canDisable: false,
    isDisabled: false,
  });
});

/**
 * Opens a new tab, opens the url bar context menu and checks that the strip-on-share menu item is not visible
 *
 * @param {string} url - The url to be loaded
 * @param {boolean} prefEnabled - Whether privacy.query_stripping.strip_on_share.enabled should be enabled for the test
 * @param {boolean} canDisable - Whether privacy.query_stripping.strip_on_share.canDisable should be enabled for the test
 * @param {boolean} selection - True: The whole url will be selected, false: Only part of the url will be selected
 */
async function testMenuItemDisabled(url, prefEnabled, canDisable, selection) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_on_share.enabled", prefEnabled],
      ["privacy.query_stripping.strip_on_share.canDisable", canDisable],
    ],
  });

  await BrowserTestUtils.withNewTab(url, async function () {
    gURLBar.focus();
    if (selection) {
      //select only part of the url
      gURLBar.selectionStart = url.indexOf("example");
      gURLBar.selectionEnd = url.indexOf("4");
    }
    let menuitem = await promiseContextualMenuitem("strip-on-share");
    Assert.ok(
      !BrowserTestUtils.isVisible(menuitem),
      "Menu item is not visible"
    );
    let hidePromise = BrowserTestUtils.waitForEvent(
      menuitem.parentElement,
      "popuphidden"
    );
    menuitem.parentElement.hidePopup();
    await hidePromise;
  });
}

/**
 * Opens a new tab, opens the url bar context menu and checks that the strip-on-share menu item is visible.
 * Checks that the stripped version of the url is copied to the clipboard.
 *
 * @param {object} options - method options
 * @param {boolean} options.selectWholeUrl - Whether the whole url should be selected
 * @param {string} options.validUrl - The original url before the stripping occurs
 * @param {string} options.strippedUrl - The expected url after stripping occurs
 * @param {boolean} options.useTestList - Whether the StripOnShare or Test list should be used
 * @param {boolean} options.canDisable - Whether privacy.query_stripping.strip_on_share.canDisable should be enabled for the test
 * @param {boolean} options.isDisabled - Whether the menu item is visible, but disabled
 */
async function testMenuItemEnabled({
  selectWholeUrl,
  validUrl,
  strippedUrl,
  useTestList,
  canDisable,
  isDisabled,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_on_share.enabled", true],
      ["privacy.query_stripping.strip_on_share.enableTestMode", useTestList],
      ["privacy.query_stripping.strip_on_share.canDisable", canDisable],
    ],
  });

  if (useTestList) {
    let testJson = {
      global: {
        queryParams: ["utm_ad"],
        topLevelSites: ["*"],
      },
      example: {
        queryParams: ["test_2", "test_1", "TEST_5"],
        topLevelSites: ["www.example.com"],
      },
      exampleNet: {
        queryParams: ["test_3", "test_4"],
        topLevelSites: ["www.example.net"],
      },
    };

    await listService.testSetList(testJson);
  }

  await BrowserTestUtils.withNewTab(validUrl, async function () {
    gURLBar.focus();
    if (selectWholeUrl) {
      gURLBar.select();
    }
    let menuitem = await promiseContextualMenuitem("strip-on-share");
    Assert.ok(BrowserTestUtils.isVisible(menuitem), "Menu item is visible");
    if (isDisabled) {
      Assert.ok(menuitem.getAttribute("disabled"), "Menu item is greyed out");
    } else {
      Assert.ok(!menuitem.getAttribute("disabled"), "Menu item is interactive");
    }

    let hidePromise = BrowserTestUtils.waitForEvent(
      menuitem.parentElement,
      "popuphidden"
    );
    // Make sure the clean copy of the link will be copied to the clipboard
    await SimpleTest.promiseClipboardChange(strippedUrl, () => {
      menuitem.closest("menupopup").activateItem(menuitem);
    });
    await hidePromise;
  });

  await SpecialPowers.popPrefEnv();
}
