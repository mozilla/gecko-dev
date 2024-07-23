/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

var gTestTab;
var gContentAPI;

function test() {
  UITourTest();
}

var tests = [
  async function test_openSearchPanel(done) {
    // If suggestions are enabled, the panel will attempt to use the network to
    // connect to the suggestions provider, causing the test suite to fail. We
    // also change the preference to display the search bar during the test.
    Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
    registerCleanupFunction(() => {
      gCUITestUtils.removeSearchBar();
      Services.prefs.clearUserPref("browser.search.suggest.enabled");
    });

    let searchbar = await gCUITestUtils.addSearchBar();
    ok(!searchbar.textbox.popupOpen, "Popup starts as closed");
    gContentAPI.openSearchPanel(() => {
      ok(searchbar.textbox.popupOpen, "Popup was opened");
      searchbar.textbox.closePopup();
      ok(!searchbar.textbox.popupOpen, "Popup was closed");
      done();
    });
  },
];
