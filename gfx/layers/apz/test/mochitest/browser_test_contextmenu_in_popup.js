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

/* import-globals-from helper_browser_test_utils.js */
// For openSelectPopup.
Services.scriptloader.loadSubScript(
  new URL("helper_browser_test_utils.js", gTestPath).href,
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
    set: [
      ["apz.popups.enabled", true],
      ["apz.popups_without_remote.enabled", true],
      ["apz.max_tap_time", 10000],
      ["ui.click_hold_context_menus.delay", 0],
    ],
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
  container.style = "width: 100px; height: 100px;";
  panel.appendChild(container);

  const contextmenuPromise = new Promise(resolve => {
    window.addEventListener("contextmenu", e => {
      e.preventDefault();
      resolve(e);
    });
  });

  registerCleanupFunction(() => {
    panel.remove();
    anchor.remove();
  });

  // Open the popup panel.
  const popupshownPromise = promiseOneEvent(panel, "popupshown");
  panel.openPopup(anchor);
  await popupshownPromise;

  const panelRect = panel.getBoundingClientRect();

  // Make sure APZ is ready in the popup.
  await promiseApzFlushedRepaints(panel);

  // Open the contextmenu by a long press event.
  await synthesizeNativeTouch(
    panel,
    10,
    10,
    SpecialPowers.DOMWindowUtils.TOUCH_CONTACT
  );

  const isWindows = getPlatform() == "windows";
  let contextmenuEvent;
  if (isWindows) {
    // On Windows contextmenu opens after the user lifted their finger from the touchscreen.
    // Wait a frame to give a chance to trigger a long-tap event.
    await promiseFrame();
    await synthesizeNativeTouch(
      panel,
      10,
      10,
      SpecialPowers.DOMWindowUtils.TOUCH_REMOVE
    );
    contextmenuEvent = await contextmenuPromise;
  } else {
    contextmenuEvent = await contextmenuPromise;
    await synthesizeNativeTouch(
      panel,
      10,
      10,
      SpecialPowers.DOMWindowUtils.TOUCH_REMOVE
    );
  }

  // The contextmenu event should be inside the popup panel.
  ok(
    contextmenuEvent.clientX >= panelRect.x,
    `${contextmenuEvent.clientX} >= ${panelRect.x}`
  );
  ok(
    contextmenuEvent.clientX <= panelRect.x + panelRect.width,
    `${contextmenuEvent.clientX} <= ${panelRect.x} + ${panelRect.width}`
  );
  ok(
    contextmenuEvent.clientY >= panelRect.y,
    `${contextmenuEvent.clientY} >= ${panelRect.y}`
  );
  ok(
    contextmenuEvent.clientY <= panelRect.y + panelRect.height,
    `${contextmenuEvent.clientY} <= ${panelRect.y} + ${panelRect.height}`
  );

  await hideSelectPopup();
});
