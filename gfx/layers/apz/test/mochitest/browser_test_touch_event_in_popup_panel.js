/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/paint_listener.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

// Cleanup for paint_listener.js.
add_task(() => {
  registerCleanupFunction(() => {
    delete window.waitForAllPaintsFlushed;
    delete window.waitForAllPaints;
    delete window.promiseAllPaintsDone;
  });
});

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [...getPrefs("TOUCH_EVENTS:PAN"), ["apz.popups.enabled", true]],
  });

  const navBar = document.getElementById("nav-bar");

  const anchor = document.createXULElement("toolbarbutton");
  anchor.classList.add("toolbarbutton-1", "chromeclass-toolbar-additional");
  navBar.appendChild(anchor);

  // Prepare a popup panel with touchstart and click event listeners.
  const panel = document.createXULElement("panel");
  panel.setAttribute("noautohide", true);
  navBar.appendChild(panel);

  const container = document.createElement("div");
  container.style = "width: 100px; height: 100px; overflow-y: scroll";
  const scrollPromise = promiseOneEvent(container, "scroll");
  panel.appendChild(container);

  const spacer = document.createElement("div");
  spacer.style.height = "500px";
  container.appendChild(spacer);

  registerCleanupFunction(() => {
    panel.remove();
    anchor.remove();
  });

  // Open the popup panel.
  const popupshownPromise = promiseOneEvent(panel, "popupshown");
  panel.openPopup(anchor);
  await popupshownPromise;

  // Make sure APZ is ready in the popup.
  await promiseApzFlushedRepaints(panel);

  await synthesizeNativeTouchDrag(container, 50, 50, 0, -20);

  await scrollPromise;

  ok(true, "Scrolling by touch events works in browser popup window");
});
