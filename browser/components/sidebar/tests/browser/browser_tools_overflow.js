/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [VERTICAL_TABS_PREF, true],
      [SIDEBAR_VISIBILITY_PREF, "always-show"],
      ["browser.ml.chat.enabled", true],
      ["browser.contextual-password-manager.enabled", true],
      ["sidebar.main.tools", "aichat,passwords,syncedtabs,history"],
    ],
  });
});
registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  cleanUpExtraTabs();
});

async function resizeTools(deltaY) {
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });

  // Let the tools splitter stabilize before attempting a drag-and-drop.
  await waitForRepaint();

  info(`Drag the tools splitter by ${deltaY} px.`);
  const { toolsSplitter: splitter } = SidebarController.sidebarMain;
  EventUtils.synthesizeMouseAtCenter(splitter, { type: "mousedown" });
  await mouseMoveInChunksVertical(splitter, deltaY, 10);
  EventUtils.synthesizeMouse(splitter, 0, 0, { type: "mouseup" });

  info(`The tools container has been expanded.`);

  AccessibilityUtils.resetEnv();
}

async function mouseMoveInChunksVertical(el, deltaY, numberOfChunks) {
  let chunkIndex = 0;
  const chunkSize = deltaY / numberOfChunks;
  const finished = Promise.withResolvers();

  function synthesizeMouseMove() {
    info(`chunkSize: ${chunkSize}`);
    // mousemove by a single chunk. Queue up the next chunk if necessary.
    EventUtils.synthesizeMouse(el, 0, chunkSize, { type: "mousemove" });
    if (++chunkIndex === numberOfChunks) {
      finished.resolve();
    } else {
      requestAnimationFrame(synthesizeMouseMove);
    }
  }

  await waitForRepaint();
  requestAnimationFrame(synthesizeMouseMove);
  await finished.promise;
}

function getToolsHeight({ SidebarController } = window) {
  return SidebarController.sidebarMain.buttonGroup.clientHeight;
}

async function resetToolsHeight() {
  // Reset tools height
  await resizeTools(-500);
  await SidebarController.sidebarMain.requestUpdate();
  await SidebarController.sidebarMain.updateComplete;
  await SidebarController.waitUntilStable();
  await BrowserTestUtils.waitForMutationCondition(
    SidebarController.sidebarMain.buttonGroup,
    { attributes: true, attributeFilter: ["overflowing"] },
    () => !SidebarController.sidebarMain.shouldShowOverflowButton
  );
}

add_task(async function test_resize_of_tools() {
  await SidebarController.initializeUIState({
    launcherExpanded: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.main.tools", "aichat,passwords,syncedtabs,history,bookmarks"],
    ],
  });

  await resetToolsHeight();
  let overflowButton = SidebarController.sidebarMain.moreToolsButton;
  Assert.ok(
    !overflowButton,
    "The overflow button is not visible before resize"
  );

  const originalHeight = getToolsHeight();
  info("Resize the tools container.");
  await resizeTools(600);
  await SidebarController.sidebarMain.updateComplete;
  await SidebarController.waitUntilStable();
  await BrowserTestUtils.waitForMutationCondition(
    SidebarController.sidebarMain.buttonGroup,
    { attributes: true, attributeFilter: ["overflowing"] },
    () => SidebarController.sidebarMain.shouldShowOverflowButton
  );
  const newHeight = getToolsHeight();
  info(`original: ${originalHeight}, new: ${newHeight}`);
  Assert.less(
    parseInt(newHeight),
    parseInt(originalHeight),
    "Tools container was resized."
  );

  overflowButton = SidebarController.sidebarMain.moreToolsButton;
  Assert.ok(
    overflowButton.checkVisibility(),
    "The overflow button is visible after resize"
  );

  await resetToolsHeight();

  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }
});

add_task(async function test_overflow_menu() {
  await SidebarController.initializeUIState({
    launcherExpanded: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.main.tools", "aichat,passwords,syncedtabs,history,bookmarks"],
    ],
  });

  await resetToolsHeight();
  let overflowButton = SidebarController.sidebarMain.moreToolsButton;
  Assert.ok(
    !overflowButton,
    "The overflow button is not visible before resize"
  );

  const originalHeight = getToolsHeight();
  info("Resize the tools container.");
  await resizeTools(600);
  await SidebarController.sidebarMain.updateComplete;
  await SidebarController.waitUntilStable();
  await BrowserTestUtils.waitForMutationCondition(
    SidebarController.sidebarMain.buttonGroup,
    { attributes: true, attributeFilter: ["overflowing"] },
    () => SidebarController.sidebarMain.shouldShowOverflowButton
  );
  const newHeight = getToolsHeight();
  info(`original: ${originalHeight}, new: ${newHeight}`);
  Assert.less(
    parseInt(newHeight),
    parseInt(originalHeight),
    "Tools container was resized."
  );

  overflowButton = SidebarController.sidebarMain.moreToolsButton;
  Assert.ok(
    overflowButton.checkVisibility(),
    "The overflow button is visible after resize"
  );

  // Open the overflow menu
  let overflowMenu = document.getElementById("sidebar-tools-overflow");
  let promiseMenuShown = BrowserTestUtils.waitForEvent(
    overflowMenu,
    "popupshown"
  );
  overflowButton.click();
  await promiseMenuShown;

  // Open the Customize Sidebar panel using the overflow menu
  let customizeSidebarButton = overflowMenu.querySelector(
    "moz-button[view=viewCustomizeSidebar]"
  );
  let promisePanelShown = BrowserTestUtils.waitForEvent(window, "SidebarShown");
  customizeSidebarButton.click();
  await promisePanelShown;
  Assert.equal(SidebarController.currentID, "viewCustomizeSidebar");

  ok(true, "Customize panel is shown.");

  // Close customize panel
  SidebarController.hide();

  await resetToolsHeight();

  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }
});

add_task(async function test_overflow_menu_with_keyboard() {
  await SidebarController.initializeUIState({
    launcherExpanded: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.main.tools", "aichat,passwords,syncedtabs,history,bookmarks"],
    ],
  });

  await resetToolsHeight();
  let overflowButton = SidebarController.sidebarMain.moreToolsButton;
  Assert.ok(
    !overflowButton,
    "The overflow button is not visible before resize"
  );

  const originalHeight = getToolsHeight();
  info("Resize the tools container.");
  await resizeTools(600);
  await SidebarController.sidebarMain.updateComplete;
  await SidebarController.waitUntilStable();
  await BrowserTestUtils.waitForMutationCondition(
    SidebarController.sidebarMain.buttonGroup,
    { attributes: true, attributeFilter: ["overflowing"] },
    () => SidebarController.sidebarMain.shouldShowOverflowButton
  );
  const newHeight = getToolsHeight();
  info(`original: ${originalHeight}, new: ${newHeight}`);
  Assert.less(
    parseInt(newHeight),
    parseInt(originalHeight),
    "Tools container was resized."
  );

  overflowButton = SidebarController.sidebarMain.moreToolsButton;
  Assert.ok(
    overflowButton.checkVisibility(),
    "The overflow button is visible after resize"
  );

  const sidebar = document.querySelector("sidebar-main");
  const newTabButton = sidebar.querySelector("#tabs-newtab-button");
  newTabButton.focus();
  ok(isActiveElement(newTabButton), "New tab button is focused again.");

  info("Tab to get to tools.");
  EventUtils.synthesizeKey("KEY_Tab", {});
  if (isActiveElement(sidebar.toolButtons[0])) {
    ok(
      isActiveElement(sidebar.toolButtons[0]),
      "First tool button is focused."
    );
    info("Tab again to reach the overflow button");
    EventUtils.synthesizeKey("KEY_Tab", {});
  }

  ok(isActiveElement(overflowButton), "Overflow button is focused.");

  // Open the overflow menu
  let overflowMenu = document.getElementById("sidebar-tools-overflow");
  let promiseMenuShown = BrowserTestUtils.waitForEvent(
    overflowMenu,
    "popupshown"
  );
  info("Press Space key.");
  EventUtils.synthesizeKey(" ", {});
  await promiseMenuShown;

  // Open the Customize Sidebar panel using the overflow menu
  let customizeSidebarButton = overflowMenu.querySelector(
    "moz-button[view=viewCustomizeSidebar]"
  );
  ok(
    isActiveElement(customizeSidebarButton),
    "Customize sidebar button is focused."
  );
  info("Press Space key.");
  EventUtils.synthesizeKey(" ", {});
  await BrowserTestUtils.waitForMutationCondition(
    overflowButton,
    { attributes: true },
    () => {
      return SidebarController.currentID === "viewCustomizeSidebar";
    }
  );

  ok(true, "Customize panel is shown.");

  // Close customize panel
  SidebarController.hide();

  await resetToolsHeight();

  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }
});

add_task(async function test_tools_overflow() {
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");
  sidebar.expanded = true;
  await sidebar.updateComplete;

  let toolsAndExtensionsButtonGroup = sidebar.shadowRoot.querySelector(
    ".tools-and-extensions"
  );
  Assert.strictEqual(
    toolsAndExtensionsButtonGroup.getAttribute("orientation"),
    "horizontal",
    "Tools are displaying horizontally"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.main.tools", "aichat,passwords,syncedtabs,history,bookmarks"],
    ],
  });
  await sidebar.updateComplete;
  Assert.strictEqual(
    toolsAndExtensionsButtonGroup.getAttribute("orientation"),
    "horizontal",
    "Tools are displaying horizontally"
  );
  for (const toolMozButton of toolsAndExtensionsButtonGroup.children) {
    ok(
      !toolMozButton.innerText.length,
      `Tool button is not displaying label text`
    );
  }
});
