/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/paint_listener.js",
  this
);

Services.scriptloader.loadSubScript(
  new URL("apz_test_utils.js", gTestPath).href,
  this
);

Services.scriptloader.loadSubScript(
  new URL("apz_test_native_event_utils.js", gTestPath).href,
  this
);

// Cleanup for paint_listener.js and hitTestConfig.
add_task(() => {
  registerCleanupFunction(() => {
    delete window.waitForAllPaintsFlushed;
    delete window.waitForAllPaints;
    delete window.promiseAllPaintsDone;
    delete window.hitTestConfig;
  });
});

// Setup preferences.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["apz.popups.enabled", true],
      ["apz.popups_without_remote.enabled", true],
      ["test.events.async.enabled", true],
      ["apz.test.logging_enabled", true],
    ],
  });
});

add_task(async () => {
  // Open a top level window having the root scroll container.
  // NOTE: Our browser window doesn't have the root scroll container.
  const dialogWindow = window.openDialog(
    // Whatever document loaded in this test would be okay, because this test
    // creates dynamically a popup element and relevant elements, but data URI
    // can not be loaded in the parent proces.
    getRootDirectory(gTestPath) + "helper_popup_menu_in_parent_process-1.html",
    null,
    // NOTE: `dialog=no` is a key factor to generate the root scroll container.
    "dialog=no,innerWidth=200,innerHeight=200"
  );
  await Promise.all([
    BrowserTestUtils.waitForEvent(dialogWindow, "load"),
    BrowserTestUtils.waitForEvent(dialogWindow, "focus"),
    BrowserTestUtils.waitForEvent(dialogWindow, "activate"),
  ]);

  await promiseOnlyApzControllerFlushed(dialogWindow);

  // Create a popup menu dynamically and set `position:fixed` to the menu.
  const popupset = dialogWindow.document.createXULElement("popupset");
  dialogWindow.document.documentElement.appendChild(popupset);
  const popup = dialogWindow.document.createXULElement("menupopup");
  popup.style.position = "fixed";
  popup.setAttribute("id", "bug1943597");
  popupset.appendChild(popup);
  const menuitem = dialogWindow.document.createXULElement("menuitem");
  menuitem.setAttribute("label", "item");
  menuitem.style = "width: 100px; height: 100px;";
  popup.appendChild(menuitem);

  // Open the popup.
  const popupshownPromise = new Promise(resolve => {
    popup.addEventListener("popupshown", resolve());
  });
  popup.openPopupAtScreen(
    dialogWindow.mozInnerScreenX,
    dialogWindow.mozInnerScreenY,
    true
  );
  await popupshownPromise;

  // Make sure APZ is ready for the popup.
  await ensureApzReadyForPopup(popup, dialogWindow);
  await promiseApzFlushedRepaints(popup);

  // Do a hit test inside the popup.
  checkHitResult(
    hitTest({ x: 50, y: 50 }, popup),
    // There's a wheel event listener in arrowscrollbox.js which is used for popup.
    APZHitResultFlags.VISIBLE | APZHitResultFlags.APZ_AWARE_LISTENERS,
    SpecialPowers.DOMWindowUtils.getViewId(popup),
    SpecialPowers.DOMWindowUtils.getLayersId(popup),
    "`position:fixed` popup"
  );

  // Close the popup.
  const popuphiddenPromise = new Promise(resolve => {
    popup.addEventListener("popuphidden", resolve());
  });
  popup.hidePopup();
  await popuphiddenPromise;

  await BrowserTestUtils.closeWindow(dialogWindow);
});
