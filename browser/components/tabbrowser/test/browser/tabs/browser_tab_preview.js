/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// NOTE on usage of sinon spies with THP components
// since THP is lazy-loaded, the tab hover preview component *must*
// be activated at least once in each test prior to setting up
// any spies against this component.
// Since each test reuses the same window, generally this issue will only
// be made evident in chaos-mode tests that run out of order (and
// thus will result in an intermittent).
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

async function openPreview(tab, win = window) {
  const previewShown = BrowserTestUtils.waitForPopupEvent(
    win.document.getElementById("tab-preview-panel"),
    "shown"
  );
  EventUtils.synthesizeMouse(tab, 1, 1, { type: "mouseover" }, win);
  return previewShown;
}

async function closePreviews(win = window) {
  const tabs = win.document.getElementById("tabbrowser-tabs");
  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    win.document.getElementById("tab-preview-panel"),
    "hidden"
  );
  EventUtils.synthesizeMouse(
    tabs,
    0,
    tabs.outerHeight + 1,
    {
      type: "mouseout",
    },
    win
  );
  return previewHidden;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.hoverPreview.enabled", true],
      ["browser.tabs.hoverPreview.showThumbnails", false],
      ["browser.tabs.tooltipsShowPidAndActiveness", false],
      ["ui.tooltip.delay_ms", 0],
    ],
  });
});

/**
 * Verify the following:
 *
 * 1. Tab preview card appears when the mouse hovers over a tab
 * 2. Tab preview card shows the correct preview for the tab being hovered
 * 3. Tab preview card is dismissed when the mouse leaves the tab bar
 */
add_task(async function hoverTests() {
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 =
    "data:text/html,<html><head><title>Second New Tab</title></head><body>Hello</body></html>";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewContainer = document.getElementById("tab-preview-panel");

  await openPreview(tab1);
  Assert.equal(
    previewContainer.querySelector(".tab-preview-title").innerText,
    "First New Tab",
    "Preview of tab1 shows correct title"
  );
  await closePreviews();

  await openPreview(tab2);
  Assert.equal(
    previewContainer.querySelector(".tab-preview-title").innerText,
    "Second New Tab",
    "Preview of tab2 shows correct title"
  );
  await closePreviews();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

// Bug 1897475 - don't show tab previews in background windows
// TODO Bug 1899556: If possible, write a test to confirm tab previews
// aren't shown when /all/ windows are in the background
add_task(async function noTabPreviewInBackgroundWindowTests() {
  const bgWindow = window;

  const bgTabUrl =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const bgTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, bgTabUrl);

  // tab must be opened at least once to ensure that bgWindow tab preview lazy loads
  await openPreview(bgTab, bgWindow);
  await closePreviews(bgWindow);

  const bgPreviewComponent = bgWindow.gBrowser.tabContainer.previewPanel;
  sinon.spy(bgPreviewComponent, "activate");

  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();
  let fgTab = fgWindow.gBrowser.tabs[0];
  let fgWindowPreviewContainer =
    fgWindow.document.getElementById("tab-preview-panel");

  await openPreview(fgTab, fgWindow);
  Assert.equal(
    fgWindowPreviewContainer.querySelector(".tab-preview-title").innerText,
    "New Tab",
    "Preview of foreground tab shows correct title"
  );
  await closePreviews(fgWindow);

  // ensure tab1 preview doesn't open, as it's now in a background window
  EventUtils.synthesizeMouseAtCenter(bgTab, { type: "mouseover" }, bgWindow);
  await BrowserTestUtils.waitForCondition(() => {
    return bgPreviewComponent.activate.calledOnce;
  });
  Assert.equal(
    bgPreviewComponent._panel.state,
    "closed",
    "preview does not open from background window"
  );

  BrowserTestUtils.removeTab(fgTab);
  await BrowserTestUtils.closeWindow(fgWindow);

  BrowserTestUtils.removeTab(bgTab);

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
  sinon.restore();
});

/**
 * Tab preview should be dismissed when a new tab is focused/selected
 */
add_task(async function focusTests() {
  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  const tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  const previewPanel = document.getElementById("tab-preview-panel");

  await openPreview(tab1);
  Assert.equal(previewPanel.state, "open", "Preview is open");

  let previewHidden = BrowserTestUtils.waitForPopupEvent(
    previewPanel,
    "hidden"
  );
  tab1.click();
  await previewHidden;
  Assert.equal(
    previewPanel.state,
    "closed",
    "Preview is closed after selecting tab"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * Verify that the pid and activeness statuses are not shown
 * when the flag is not enabled.
 */
add_task(async function pidAndActivenessHiddenByDefaultTests() {
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const previewContainer = document.getElementById("tab-preview-panel");

  await openPreview(tab1);
  Assert.equal(
    previewContainer.querySelector(".tab-preview-pid").innerText,
    "",
    "Tab PID is not shown"
  );
  Assert.equal(
    previewContainer.querySelector(".tab-preview-activeness").innerText,
    "",
    "Tab activeness is not shown"
  );

  await closePreviews();

  BrowserTestUtils.removeTab(tab1);

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

add_task(async function pidAndActivenessTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.tooltipsShowPidAndActiveness", true]],
  });

  const tabUrl1 =
    "data:text/html,<html><head><title>Single process tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 = `data:text/html,<html>
      <head>
        <title>Multi-process tab</title>
      </head>
      <body>
        <iframe
          id="inlineFrameExample"
          title="Inline Frame Example"
          width="300"
          height="200"
          src="https://example.com">
        </iframe>
      </body>
    </html>`;
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewContainer = document.getElementById("tab-preview-panel");

  await openPreview(tab1);
  Assert.stringMatches(
    previewContainer.querySelector(".tab-preview-pid").innerText,
    /^pid: \d+$/,
    "Tab PID is shown on single process tab"
  );
  Assert.equal(
    previewContainer.querySelector(".tab-preview-activeness").innerText,
    "",
    "Tab activeness is not shown on inactive tab"
  );
  await closePreviews();

  await openPreview(tab2);
  Assert.stringMatches(
    previewContainer.querySelector(".tab-preview-pid").innerText,
    /^pids: \d+, \d+$/,
    "Tab PIDs are shown on multi-process tab"
  );
  Assert.equal(
    previewContainer.querySelector(".tab-preview-activeness").innerText,
    "[A]",
    "Tab activeness is shown on active tab"
  );
  await closePreviews();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await SpecialPowers.popPrefEnv();

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * Verify that non-selected tabs display a thumbnail in their preview
 * when browser.tabs.hoverPreview.showThumbnails is set to true,
 * while the currently selected tab never displays a thumbnail in its preview.
 */
add_task(async function thumbnailTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.hoverPreview.showThumbnails", true]],
  });
  const tabUrl1 = "about:blank";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 = "about:blank";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewPanel = document.getElementById("tab-preview-panel");

  let thumbnailUpdated = BrowserTestUtils.waitForEvent(
    previewPanel,
    "previewThumbnailUpdated",
    false,
    evt => evt.detail.thumbnail
  );
  await openPreview(tab1);
  await thumbnailUpdated;
  Assert.ok(
    previewPanel.querySelectorAll(
      ".tab-preview-thumbnail-container img, .tab-preview-thumbnail-container canvas"
    ).length,
    "Tab1 preview contains thumbnail"
  );

  await closePreviews();
  thumbnailUpdated = BrowserTestUtils.waitForEvent(
    previewPanel,
    "previewThumbnailUpdated"
  );
  await openPreview(tab2);
  await thumbnailUpdated;
  Assert.equal(
    previewPanel.querySelectorAll(
      ".tab-preview-thumbnail-container img, .tab-preview-thumbnail-container canvas"
    ).length,
    0,
    "Tab2 (selected) does not contain thumbnail"
  );

  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    previewPanel,
    "hidden"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await SpecialPowers.popPrefEnv();

  // Removing the tab should close the preview.
  await previewHidden;

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * make sure delay is applied when mouse leaves tabstrip
 * but not when moving between tabs on the tabstrip
 */
add_task(async function delayTests() {
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const tabUrl2 =
    "data:text/html,<html><head><title>Second New Tab</title></head><body>Hello</body></html>";
  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl2);
  const previewElement = document.getElementById("tab-preview-panel");

  await openPreview(tab1);

  const previewComponent = gBrowser.tabContainer.previewPanel;
  sinon.spy(previewComponent, "deactivate");

  // I can't fake this like in hoverTests, need to send an updated-tab signal
  //await openPreview(tab2);

  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    previewElement,
    "hidden"
  );
  Assert.ok(
    !previewComponent.deactivate.called,
    "Delay is not reset when moving between tabs"
  );

  EventUtils.synthesizeMouseAtCenter(document.getElementById("reload-button"), {
    type: "mousemove",
  });

  await previewHidden;

  Assert.ok(
    previewComponent.deactivate.called,
    "Delay is reset when cursor leaves tabstrip"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  sinon.restore();
});

/**
 * Dragging a tab should deactivate the preview
 */
add_task(async function dragTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["ui.tooltip.delay_ms", 1000]],
  });
  const tabUrl1 =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl1);
  const previewElement = document.getElementById("tab-preview-panel");

  await openPreview(tab1);

  const previewComponent = gBrowser.tabContainer.previewPanel;
  sinon.spy(previewComponent, "deactivate");

  const previewHidden = BrowserTestUtils.waitForPopupEvent(
    previewElement,
    "hidden"
  );
  let dragend = BrowserTestUtils.waitForEvent(tab1, "dragend");
  EventUtils.synthesizePlainDragAndDrop({
    srcElement: tab1,
    destElement: null,
    stepX: 100,
    stepY: 0,
  });

  await previewHidden;

  Assert.ok(
    previewComponent.deactivate.called,
    "delay is reset after drag started"
  );

  await dragend;
  BrowserTestUtils.removeTab(tab1);
  sinon.restore();

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });

  await SpecialPowers.popPrefEnv();
});

/**
 * Other open context menus should prevent tab preview from opening
 */
add_task(async function panelSuppressionOnContextMenuTests() {
  const tabUrl =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

  // tab must be opened at least once to ensure that tab preview lazy loads
  await openPreview(tab);
  await closePreviews();

  const previewComponent = gBrowser.tabContainer.previewPanel;
  sinon.spy(previewComponent, "activate");

  const contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu"
  );
  const contextMenuShown = BrowserTestUtils.waitForPopupEvent(
    contentAreaContextMenu,
    "shown"
  );

  EventUtils.synthesizeMouseAtCenter(
    document.documentElement,
    { type: "contextmenu" },
    window
  );
  await contextMenuShown;

  EventUtils.synthesizeMouseAtCenter(tab, { type: "mouseover" }, window);

  await BrowserTestUtils.waitForCondition(() => {
    return previewComponent.activate.called;
  });
  Assert.equal(previewComponent._panel.state, "closed", "");

  contentAreaContextMenu.hidePopup();
  BrowserTestUtils.removeTab(tab);
  sinon.restore();

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * Other open panels should prevent tab preview from opening
 */
add_task(async function panelSuppressionOnPanelTests() {
  const tabUrl =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

  // tab must be opened at least once to ensure that tab preview lazy loads
  await openPreview(tab);
  await closePreviews();

  const previewComponent = gBrowser.tabContainer.previewPanel;
  sinon.spy(previewComponent, "activate");

  // The `openPopup` API appears to not be working for this panel,
  // but it can be triggered by firing a click event on the associated button.
  const appMenuButton = document.getElementById("PanelUI-menu-button");
  const appMenuPopup = document.getElementById("appMenu-popup");
  appMenuButton.click();

  EventUtils.synthesizeMouseAtCenter(tab, { type: "mouseover" }, window);

  await BrowserTestUtils.waitForCondition(() => {
    return previewComponent.activate.calledOnce;
  });
  Assert.equal(previewComponent._panel.state, "closed", "");

  // Reset state: close the app menu popup and move the mouse off the tab
  const tabs = window.document.getElementById("tabbrowser-tabs");
  EventUtils.synthesizeMouse(
    tabs,
    0,
    tabs.outerHeight + 1,
    {
      type: "mouseout",
    },
    window
  );

  const popupHidingEvent = BrowserTestUtils.waitForEvent(
    appMenuPopup,
    "popuphiding"
  );
  appMenuPopup.hidePopup();
  await popupHidingEvent;

  // Attempt to open the tab preview immediately after the popup hiding event
  await openPreview(tab);
  Assert.equal(previewComponent._panel.state, "open", "");

  BrowserTestUtils.removeTab(tab);
  sinon.restore();

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * Ensure that the panel does not open when other panels are active or are in the process of being activated,
 * when THP is being called for the first time (lazy-loaded)
 */
add_task(async function panelSuppressionOnPanelLazyLoadTests() {
  // This needs to be done in a new window to ensure that
  // the previewPanel is being loaded for the first time
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();
  let fgTab = fgWindow.gBrowser.tabs[0];

  // The `openPopup` API appears to not be working for this panel,
  // but it can be triggered by firing a click event on the associated button.
  const appMenuButton = fgWindow.document.getElementById("PanelUI-menu-button");
  const appMenuPopup = fgWindow.document.getElementById("appMenu-popup");
  appMenuButton.click();

  EventUtils.synthesizeMouseAtCenter(fgTab, { type: "mouseover" }, fgWindow);

  await BrowserTestUtils.waitForCondition(() => {
    // Sometimes the tests run slower than the test browser -- it's not always possible
    // to catch the panel in its opening state, so we have to check for both states.
    return (
      (appMenuPopup.getAttribute("animating") === "true" ||
        appMenuPopup.getAttribute("panelopen") === "true") &&
      fgWindow.gBrowser.tabContainer.previewPanel !== null
    );
  });
  const previewComponent = fgWindow.gBrowser.tabContainer.previewPanel;

  // We can't spy on the previewComponent and check for calls to `activate` like in other tests,
  // since we can't guarantee that the spy will be set up before the call is made.
  // Therefore the only realiable way to test that the popup isn't open is to reach in and check
  // that it is in a disabled state.
  Assert.equal(previewComponent._isDisabled(), true, "");

  // Reset state: close the app menu popup and move the mouse off the tab
  const tabs = fgWindow.document.getElementById("tabbrowser-tabs");
  EventUtils.synthesizeMouse(
    tabs,
    0,
    tabs.outerHeight + 1,
    {
      type: "mouseout",
    },
    fgWindow
  );

  const popupHidingEvent = BrowserTestUtils.waitForEvent(
    appMenuPopup,
    "popuphiding"
  );
  appMenuPopup.hidePopup();
  await popupHidingEvent;

  BrowserTestUtils.removeTab(fgTab);

  // Move the mouse outside of the tab strip.
  await BrowserTestUtils.closeWindow(fgWindow);
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

/**
 * preview should be hidden if it is showing when the URLBar receives input
 */
add_task(async function urlBarInputTests() {
  const previewElement = document.getElementById("tab-preview-panel");
  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await openPreview(tab1);
  gURLBar.focus();
  Assert.equal(previewElement.state, "open", "Preview is open");

  let previewHidden = BrowserTestUtils.waitForEvent(
    previewElement,
    "popuphidden"
  );
  EventUtils.sendChar("q", window);
  await previewHidden;

  Assert.equal(previewElement.state, "closed", "Preview is closed");
  await closePreviews();
  await openPreview(tab1);
  Assert.equal(previewElement.state, "open", "Preview is open");

  previewHidden = BrowserTestUtils.waitForEvent(previewElement, "popuphidden");
  EventUtils.sendChar("q", window);
  await previewHidden;
  Assert.equal(previewElement.state, "closed", "Preview is closed");

  BrowserTestUtils.removeTab(tab1);
});

/**
 * Quickly moving the mouse off and back on to the tab strip should
 * not reset the delay
 */
add_task(async function zeroDelayTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["ui.tooltip.delay_ms", 1000]],
  });

  const tabUrl =
    "data:text/html,<html><head><title>First New Tab</title></head><body>Hello</body></html>";
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

  await openPreview(tab);
  await closePreviews();

  let resolved = false;
  let openPreviewPromise = openPreview(tab).then(() => {
    resolved = true;
  });
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  let timeoutPromise = new Promise(resolve => setTimeout(resolve, 300));
  await Promise.race([openPreviewPromise, timeoutPromise]);

  Assert.ok(resolved, "Zero delay is set immediately after leaving tab strip");

  await closePreviews();
  BrowserTestUtils.removeTab(tab);

  await SpecialPowers.popPrefEnv();
});

/**
 * The panel should be configured to roll up on wheel events if
 * the tab strip is overflowing.
 */
add_task(async function wheelTests() {
  const previewPanel = document.getElementById("tab-preview-panel");
  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  Assert.ok(
    !previewPanel.hasAttribute("rolluponmousewheel"),
    "Panel does not have rolluponmousewheel when no overflow"
  );

  let scrollOverflowEvent = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer.arrowScrollbox,
    "overflow"
  );
  BrowserTestUtils.overflowTabs(registerCleanupFunction, window, {
    overflowAtStart: false,
  });
  await scrollOverflowEvent;
  await openPreview(tab1);

  Assert.equal(
    previewPanel.getAttribute("rolluponmousewheel"),
    "true",
    "Panel has rolluponmousewheel=true when tabs overflow"
  );

  // Clean up extra tabs
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }

  // Move the mouse outside of the tab strip.
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {
    type: "mouseover",
  });
});

add_task(async function appearsAsTooltipToAccessibilityToolsTests() {
  const previewPanel = document.getElementById("tab-preview-panel");
  Assert.equal(
    previewPanel.getAttribute("role"),
    "tooltip",
    "The panel appears as a tooltip to assistive technology"
  );
});

/**
 * Verify that if the browser document title (i.e. tab label) changes,
 * the tab preview panel is updated
 */
add_task(async function tabContentChangeTests() {
  const previewPanel = document.getElementById("tab-preview-panel");

  const tabUrl =
    "data:text/html,<html><head><title>Original Tab Title</title></head><body>Hello</body></html>";
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, tabUrl);

  await openPreview(tab);
  Assert.equal(
    previewPanel.querySelector(".tab-preview-title").innerText,
    "Original Tab Title",
    "Preview of tab shows original tab title"
  );

  tab.setAttribute("label", "New Tab Title");

  await BrowserTestUtils.waitForCondition(() => {
    return (
      previewPanel.querySelector(".tab-preview-title").innerText ===
      "New Tab Title"
    );
  });

  Assert.equal(
    previewPanel.querySelector(".tab-preview-title").innerText,
    "New Tab Title",
    "Preview of tab shows new tab title"
  );

  await closePreviews();
  BrowserTestUtils.removeTab(tab);
});
