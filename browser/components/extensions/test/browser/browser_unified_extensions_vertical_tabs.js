/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the behaviour of extensions in tabstrip and extensions panel with
 * vertical tabs enabled and disabled.
 */

"use strict";

loadTestSubscript("head_unified_extensions.js");

async function installTabstripExtension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        browser_style: true,
        default_area: "tabstrip",
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  return extension;
}

add_setup(async function () {
  registerCleanupFunction(async () => {
    await CustomizableUI.reset();
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", false],
    ],
  });
});

/**
 * Tests that extension widgets are moved between the tabstrip toolbar and the unified
 * extensions menu when vertical tabs are enabled and disabled.
 */
add_task(async function test_widgets_in_tabstrip() {
  const extension = await installTabstripExtension();
  const tabstripCUITarget = CustomizableUI.getCustomizationTarget(
    document.querySelector("#TabsToolbar")
  );
  const actionNode = tabstripCUITarget.querySelector(
    ".webextension-browser-action"
  );
  is(
    actionNode && actionNode.dataset.extensionid,
    extension.id,
    "Found the installed extension in the tabstrip toolbar"
  );

  let widgetId = AppUiTestInternals.getBrowserActionWidgetId(extension.id);
  ok(
    CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_TABSTRIP).includes(
      widgetId
    ),
    "Extension widget is in the tabstrip"
  );

  // enable vertical tabs, confirm toolbar is hidden and widget nodes have moved to the extensions menu
  let toolbarChanged = BrowserTestUtils.waitForEvent(
    window,
    "toolbarvisibilitychange"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", true]],
  });
  await toolbarChanged;

  ok(
    BrowserTestUtils.isHidden(tabstripCUITarget),
    "The tabstrip toolbar is now hidden"
  );
  // Note: the gUnifiedExtensions implementation leaves the widget in the customizable
  // area, but moves/removes the nodes so it looks empty
  ok(
    !tabstripCUITarget.querySelector(".webextension-browser-action"),
    "There are no webextension widget nodes in the tabstrip toolbar anymore"
  );
  ok(
    CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_TABSTRIP).includes(
      widgetId
    ),
    "CUI still thinks the extension widget is in the tabstrip"
  );

  await openExtensionsPanel();
  ok(
    getUnifiedExtensionsItem(extension.id),
    "The extension is represented as an item in the unified extensions panel"
  );
  await closeExtensionsPanel();

  // disable vertical tabs, confirm toolbar is visible and widgets have been restored
  // to the tabstrip toolbar
  toolbarChanged = BrowserTestUtils.waitForEvent(
    window,
    "toolbarvisibilitychange"
  );
  await SpecialPowers.popPrefEnv();
  await toolbarChanged;

  ok(
    BrowserTestUtils.isVisible(tabstripCUITarget),
    "The tabstrip toolbar is now visible"
  );
  ok(
    tabstripCUITarget.querySelector(".webextension-browser-action"),
    "The webextension widget is back in the tabstrip toolbar"
  );
  ok(
    CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_TABSTRIP).includes(
      widgetId
    ),
    "Extension widget is still in the tabstrip"
  );

  // Ensure the widget is still customizable and can be moved to another area
  ok(
    !document.querySelector("#nav-bar .webextension-browser-action"),
    "There are currently no webextension widgets in the nav-bar toolbar"
  );

  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR);
  ok(
    document.querySelector("#nav-bar .webextension-browser-action"),
    "The webextension widget is now in the nav-bar toolbar"
  );
  ok(
    CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR).includes(
      widgetId
    ),
    "Extension widget is now in the nav-bar"
  );

  await extension.unload();
  await CustomizableUI.reset();
});

async function unpinFromToolbar(extension, win = window) {
  const contextMenu = await openUnifiedExtensionsContextMenu(extension.id, win);
  const pinToToolbarItem = contextMenu.querySelector(
    ".unified-extensions-context-menu-pin-to-toolbar"
  );
  ok(pinToToolbarItem, "expected 'pin to toolbar' menu item");
  is(
    pinToToolbarItem.getAttribute("checked"),
    "true",
    "pin menu item is currently checked"
  );
  const hidden = BrowserTestUtils.waitForEvent(
    win.gUnifiedExtensions.panel,
    "popuphidden",
    true
  );
  contextMenu.activateItem(pinToToolbarItem);
  await hidden;
}

/**
 * Tests that extension widgets which un-pinned from the tabstrip while it is hidden
 * remain that way when the vertical tabs are disabled and that toolbar is made visble.
 */
add_task(async function test_unpin_from_tabstrip_while_hidden() {
  const tabstripCUITarget = document.querySelector("#TabsToolbar");
  const extension = await installTabstripExtension();
  const widgetId = AppUiTestInternals.getBrowserActionWidgetId(extension.id);
  is(
    CustomizableUI.getPlacementOfWidget(widgetId)?.area,
    CustomizableUI.AREA_TABSTRIP,
    `widget located in correct area`
  );

  let toolbarChanged = BrowserTestUtils.waitForEvent(
    window,
    "toolbarvisibilitychange"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", true]],
  });
  await toolbarChanged;
  ok(
    BrowserTestUtils.isHidden(tabstripCUITarget),
    "The tabstrip area is hidden"
  );

  // Unpin the extension using the context menu on the item in the panel
  await openExtensionsPanel();
  ok(
    getUnifiedExtensionsItem(extension.id),
    "The extension is represented as an item in the unified extensions panel"
  );

  await unpinFromToolbar(extension, window);
  is(
    window.gUnifiedExtensions.panel.state,
    "closed",
    "The panel was closed by the context menu action"
  );

  is(
    CustomizableUI.getPlacementOfWidget(widgetId)?.area,
    CustomizableUI.AREA_ADDONS,
    `widget moved to the addons area`
  );

  // disable vertical tabs, confirm the widget doesnt get restored to the toolbar
  toolbarChanged = BrowserTestUtils.waitForEvent(
    window,
    "toolbarvisibilitychange"
  );
  // toggle the sidebar.verticalTabs pref to off
  await SpecialPowers.popPrefEnv();
  await toolbarChanged;

  ok(
    !tabstripCUITarget.querySelector(".webextension-browser-action"),
    "There are no extension widgets in the tabstrip"
  );

  let widget = getBrowserActionWidget(extension);
  is(
    CustomizableUI.getPlacementOfWidget(widget.id)?.area,
    CustomizableUI.AREA_ADDONS,
    `widget remained in correct area`
  );

  await extension.unload();
  await CustomizableUI.reset();
});
