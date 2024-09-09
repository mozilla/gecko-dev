/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test ensures dismissing a tab preview does not hide the nav toolbox
// when browser.fullscreen.autohide is true.

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.fullscreen.autohide", true],
      ["browser.tabs.hoverPreview.enabled", true],
      ["ui.tooltip.delay_ms", 0],
    ],
  });
});

add_task(async function testTabPreview() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let onFullscreen = Promise.all([
    BrowserTestUtils.waitForEvent(window, "fullscreen"),
    BrowserTestUtils.waitForEvent(
      window,
      "sizemodechange",
      false,
      () => window.fullScreen
    ),
  ]);
  document.getElementById("View:FullScreen").doCommand();
  await onFullscreen;
  // make sure the toolbox is visible if it's autohidden
  FullScreen.showNavToolbox();

  let tabPreviewPanel = document.getElementById("tab-preview-panel");
  // open tab preview
  const previewShown = BrowserTestUtils.waitForPopupEvent(
    tabPreviewPanel,
    "shown"
  );
  EventUtils.synthesizeMouse(
    tab,
    1,
    1,
    {
      type: "mouseover",
    },
    window
  );
  await previewShown;
  // close tab preview
  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    tabPreviewPanel,
    "hidden"
  );
  EventUtils.synthesizeMouse(
    document.getElementById("tabs-newtab-button"),
    1,
    1,
    { type: "mouseover" }
  );
  await previewHidden;
  // navtoolbox should still be visible
  Assert.ok(
    !FullScreen._isChromeCollapsed,
    "Toolbar remains visible after tab preview is hidden"
  );
  BrowserTestUtils.removeTab(tab);
  let onExitFullscreen = Promise.all([
    BrowserTestUtils.waitForEvent(window, "fullscreen"),
    BrowserTestUtils.waitForEvent(
      window,
      "sizemodechange",
      false,
      () => !window.fullScreen
    ),
  ]);
  document.getElementById("View:FullScreen").doCommand();
  await onExitFullscreen;
});
