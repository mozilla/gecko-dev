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
      ["apz.test.logging_enabled", true],
    ],
  });
});

// Create a popup having a sub scroll container.
function setupPopup(aWindow) {
  const popupset = aWindow.document.createXULElement("popupset");
  aWindow.document.documentElement.appendChild(popupset);
  const popup = aWindow.document.createXULElement("menupopup");
  popupset.appendChild(popup);

  const scroller = aWindow.document.createElement("div");
  // To make this test fail without the proper fix, this scroll container
  // height needs to be slightly taller than a value multiplying by 128px
  // (i.e. displayport alignment).
  scroller.style =
    "width: 100px; height: 550px; overflow: auto; background-color: white;";
  scroller.setAttribute("id", "bug1948522");
  popup.appendChild(scroller);

  const spacer = aWindow.document.createElement("div");
  spacer.style = "width: 200px; height: 600px; background-color: green;";
  scroller.appendChild(spacer);

  return popup;
}

add_task(async () => {
  const dialogWindow = window.openDialog(
    // Whatever document loaded in this test would be okay, because this test
    // creates dynamically a popup element and relevant elements, but data URI
    // can not be loaded in the parent process.
    getRootDirectory(gTestPath) + "helper_popup_menu_in_parent_process-1.html",
    null,
    "dialog=no,innerWidth=200,innerHeight=20"
  );
  await Promise.all([
    BrowserTestUtils.waitForEvent(dialogWindow, "load"),
    BrowserTestUtils.waitForEvent(dialogWindow, "focus"),
    BrowserTestUtils.waitForEvent(dialogWindow, "activate"),
  ]);

  await promiseOnlyApzControllerFlushed(dialogWindow);

  const popup = setupPopup(dialogWindow);

  // Open the popup.
  const popupshownPromise = new Promise(resolve => {
    popup.addEventListener("popupshown", resolve());
  });
  popup.openPopupAtScreen(
    dialogWindow.mozInnerScreenX,
    dialogWindow.mozInnerScreenY
  );
  await popupshownPromise;

  await ensureApzReadyForPopup(popup, dialogWindow);
  await promiseApzFlushedRepaints(popup);

  let displayport = getLastContentDisplayportFor("bug1948522", {
    popupElement: popup,
  });
  is(
    displayport.height,
    600,
    `the height of the displayport ${displayport.height}px should equal to 600px`
  );

  // Close the popup.
  const popuphiddenPromise = new Promise(resolve => {
    popup.addEventListener("popuphidden", resolve());
  });
  popup.hidePopup();
  await popuphiddenPromise;

  await BrowserTestUtils.closeWindow(dialogWindow);
});
