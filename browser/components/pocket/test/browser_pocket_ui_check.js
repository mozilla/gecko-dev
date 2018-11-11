"use strict";

add_task(async function test_setup() {
  let clearValue = Services.prefs.prefHasUserValue("extensions.pocket.enabled");
  let enabledOnStartup = Services.prefs.getBoolPref("extensions.pocket.enabled");
  registerCleanupFunction(() => {
    if (clearValue) {
      Services.prefs.clearUserPref("extensions.pocket.enabled");
    } else {
      Services.prefs.setBoolPref("extensions.pocket.enabled", enabledOnStartup);
    }
  });
});

add_task(async function() {
  await promisePocketEnabled();

  checkWindowProperties(true, ["Pocket", "pktUI", "pktUIMessaging"]);
  checkElements(true, ["pocket-button", "appMenu-library-pocket-button"]);

  // check context menu exists
  info("checking content context menu");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/browser/browser/components/pocket/test/test.html");

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  await BrowserTestUtils.synthesizeMouseAtCenter("body", {
    type: "contextmenu",
    button: 2,
  }, tab.linkedBrowser);
  await popupShown;

  checkElements(true, ["context-pocket", "context-savelinktopocket"]);

  contextMenu.hidePopup();
  await popupHidden;
  BrowserTestUtils.removeTab(tab);

  await promisePocketDisabled();

  checkWindowProperties(false, ["Pocket", "pktUI", "pktUIMessaging"]);
  checkElements(false, ["pocket-button", "appMenu-library-pocket-button",
                        "context-pocket", "context-savelinktopocket"]);

  await promisePocketReset();
});
