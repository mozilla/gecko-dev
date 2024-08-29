/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { PlacesSyncUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesSyncUtils.sys.mjs"
);

add_setup(async function () {
  // There should be at least one bookmark in the mobile root.
  await PlacesUtils.bookmarks.insert({
    url: "https://www.mozilla.org/",
    parentGuid: PlacesUtils.bookmarks.mobileGuid,
  });
  await PlacesSyncUtils.bookmarks.ensureMobileQuery();
  CustomizableUI.addWidgetToArea(
    "bookmarks-menu-button",
    CustomizableUI.AREA_NAVBAR,
    4
  );

  registerCleanupFunction(async function () {
    CustomizableUI.reset();
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test() {
  for (let { popup, mobileMenuitem } of [
    {
      popup: document.getElementById("BMB_bookmarksPopup"),
      mobileMenuitem: document.getElementById("BMB_mobileBookmarks"),
    },
    {
      popup: document.getElementById("bookmarksMenuPopup"),
      mobileMenuitem: document.getElementById("menu_mobileBookmarks"),
    },
  ]) {
    info("Open bookmarks popup.");
    let shownPromise = BrowserTestUtils.waitForEvent(
      popup,
      "popupshowing",
      false,
      e => e.target == popup
    );
    // Simulates popup opening causing it to populate. We cannot just open
    // normally since it would not work on Mac native menubar.
    popup.dispatchEvent(
      new MouseEvent("popupshowing", {
        bubbles: true,
      })
    );
    await shownPromise;

    Assert.ok(!mobileMenuitem.hidden, "Check mobile root is not hidden.");
  }
});
