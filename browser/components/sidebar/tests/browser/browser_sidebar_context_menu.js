/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  // turn off animations for this test
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.animation.enabled", false]],
  });
});

const initialTabDirection = Services.prefs.getBoolPref("sidebar.verticalTabs")
  ? "vertical"
  : "horizontal";

add_task(async function test_extension_context_menu() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await waitForBrowserWindowActive(win);
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  await sidebar.updateComplete;
  ok(BrowserTestUtils.isVisible(sidebar), "Sidebar is shown.");

  const manageStub = sinon.stub(sidebar, "manageExtension");
  const reportStub = sinon.stub(sidebar, "reportExtension");
  const removeStub = sinon.stub(sidebar, "removeExtension");

  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  // TODO: Once `sidebar.revamp` is either enabled by default, or removed
  // entirely, this test should run in the current window, and it should only
  // await one "sidebar" message. Bug 1896421
  await extension.awaitMessage("sidebar");
  await extension.awaitMessage("sidebar");
  is(sidebar.extensionButtons.length, 1, "Extension is shown in the sidebar.");

  const contextMenu = document.getElementById("sidebar-context-menu");
  is(contextMenu.state, "closed", "Checking if context menu is closed");

  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      // Check sidebar context menu buttons are hidden
      () => {
        ok(
          document.getElementById("sidebar-context-menu-hide-sidebar").hidden,
          "Hide sidebar button is hidden"
        );
        ok(
          document.getElementById("sidebar-context-menu-enable-vertical-tabs")
            .hidden,
          "Enable vertical tabs button is hidden"
        );
        ok(
          document.getElementById("sidebar-context-menu-customize-sidebar")
            .hidden,
          "Customize sidebar button is hidden"
        );
      };
    }
  );

  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      // Click "Manage Extension"
      const manageExtensionButtonEl = document.getElementById(
        "sidebar-context-menu-manage-extension"
      );
      manageExtensionButtonEl.click();
    }
  );
  ok(manageStub.called, "Manage Extension called");

  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      // Click "Report Extension"
      const reportExtensionButtonEl = document.getElementById(
        "sidebar-context-menu-report-extension"
      );
      reportExtensionButtonEl.click();
    }
  );
  ok(reportStub.called, "Report Extension called");

  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      // Click "Remove Extension"
      const removeExtensionButtonEl = document.getElementById(
        "sidebar-context-menu-remove-extension"
      );
      removeExtensionButtonEl.click();
    }
  );
  ok(removeStub.called, "Remove Extension called");

  info(
    "Verify report context menu disabled/enabled based on about:config pref"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.enabled", false]],
  });
  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      const reportExtensionButtonEl = document.getElementById(
        "sidebar-context-menu-report-extension"
      );
      is(
        reportExtensionButtonEl.disabled,
        true,
        "Expect report item to be disabled"
      );
    }
  );
  await SpecialPowers.popPrefEnv();
  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      const reportExtensionButtonEl = document.getElementById(
        "sidebar-context-menu-report-extension"
      );
      is(
        reportExtensionButtonEl.disabled,
        false,
        "Expect report item to be enabled"
      );
    }
  );

  info(
    "Verify remove context menu disabled/enabled based on addon uninstall permission"
  );
  const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
    "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
  );
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      Extensions: {
        Locked: [extension.id],
      },
    },
  });
  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      const removeExtensionButtonEl = document.getElementById(
        "sidebar-context-menu-remove-extension"
      );
      is(
        removeExtensionButtonEl.disabled,
        true,
        "Expect remove item to be disabled"
      );
    }
  );
  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  await openAndWaitForContextMenu(
    contextMenu,
    sidebar.extensionButtons[0],
    () => {
      const removeExtensionButtonEl = document.getElementById(
        "sidebar-context-menu-remove-extension"
      );
      is(
        removeExtensionButtonEl.disabled,
        false,
        "Expect remove item to be enabled"
      );
    }
  );

  sinon.restore();
  await extension.unload();
  ok(
    BrowserTestUtils.isHidden(sidebar),
    "Unloading the extension causes the sidebar launcher to hide"
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_sidebar_context_menu() {
  const { document, SidebarController } = window;
  const { sidebarMain, sidebarContainer } = SidebarController;
  await SidebarController.initializeUIState({
    launcherVisible: true,
  });
  ok(BrowserTestUtils.isVisible(sidebarMain), "Sidebar is shown.");

  const contextMenu = document.getElementById("sidebar-context-menu");
  is(contextMenu.state, "closed", "Checking if context menu is closed");

  await openAndWaitForContextMenu(contextMenu, sidebarMain, () => {
    // Check extension context menu buttons are hidden
    () => {
      ok(
        document.getElementById("sidebar-context-menu-report-extension").hidden,
        "Report extension button is hidden"
      );
      ok(
        document.getElementById("sidebar-context-menu-remove-extension").hidden,
        "Remove extension tabs button is hidden"
      );
      ok(
        document.getElementById("sidebar-context-menu-manage-extension").hidden,
        "Manage extension button is hidden"
      );
    };
  });

  await openAndWaitForContextMenu(contextMenu, sidebarMain, () => {
    // Click customize sidebar
    const customizeSidebarMenuItem = document.getElementById(
      "sidebar-context-menu-customize-sidebar"
    );
    customizeSidebarMenuItem.click();
  });
  is(
    SidebarController.currentID,
    "viewCustomizeSidebar",
    "Customize sidebar panel is open"
  );

  await openAndWaitForContextMenu(contextMenu, sidebarMain, () => {
    // Click hide sidebar
    const hideSidebarMenuItem = document.getElementById(
      "sidebar-context-menu-hide-sidebar"
    );
    hideSidebarMenuItem.click();
  });
  ok(sidebarContainer.hidden, "Sidebar is not visible");
  ok(!SidebarController.isOpen, "Sidebar panel is closed");
  SidebarController._state.updateVisibility(true);

  await openAndWaitForContextMenu(contextMenu, sidebarMain, () => {
    // Click turn on vertical tabs
    const enableVerticalTabsMenuItem = document.getElementById(
      "sidebar-context-menu-enable-vertical-tabs"
    );
    enableVerticalTabsMenuItem.click();
  });
  ok(
    Services.prefs.getBoolPref("sidebar.verticalTabs", false),
    "Vertical tabs disabled"
  );
  Services.prefs.clearUserPref("sidebar.verticalTabs");
  await waitForTabstripOrientation(initialTabDirection);

  is(contextMenu.state, "closed", "Context menu closed for vertical tabs");
});

add_task(async function test_toggle_vertical_tabs_from_sidebar_button() {
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", false]],
  });
  await waitForTabstripOrientation("horizontal");
  Assert.equal(
    Services.prefs.getStringPref("sidebar.visibility"),
    "hide-sidebar",
    "Sanity check the visibilty pref when verticalTabs are disabled"
  );

  info("Enable vertical tabs from right clicking the sidebar-button");
  const toolbarContextMenu = document.getElementById("toolbar-context-menu");
  const toggleMenuItem = document.getElementById(
    "toolbar-context-toggle-vertical-tabs"
  );
  const customizeSidebarItem = document.getElementById(
    "toolbar-context-customize-sidebar"
  );
  const sidebarButton = document.getElementById("sidebar-button");
  await openAndWaitForContextMenu(toolbarContextMenu, sidebarButton, () => {
    Assert.deepEqual(
      document.l10n.getAttributes(toggleMenuItem),
      { id: "toolbar-context-turn-on-vertical-tabs", args: null },
      "Context menu item indicates that it enables vertical tabs."
    );
    toggleMenuItem.click();
  });
  await waitForTabstripOrientation("vertical");
  await TestUtils.waitForCondition(
    () => gBrowser.tabContainer.verticalMode,
    "Vertical tabs are enabled."
  );
  Assert.equal(
    Services.prefs.getStringPref("sidebar.visibility"),
    "always-show",
    "Sanity check the visibilty pref when verticalTabs are enabled"
  );

  // Open customize sidebar panel from context menu
  await openAndWaitForContextMenu(toolbarContextMenu, sidebarButton, () => {
    customizeSidebarItem.click();
  });
  ok(window.SidebarController.isOpen, "Sidebar is open");
  Assert.equal(
    window.SidebarController.currentID,
    "viewCustomizeSidebar",
    "Sidebar should have opened to the customize sidebar panel"
  );

  info("Disable vertical tabs from right clicking the sidebar-button");
  await openAndWaitForContextMenu(toolbarContextMenu, sidebarButton, () => {
    Assert.deepEqual(
      document.l10n.getAttributes(toggleMenuItem),
      { id: "toolbar-context-turn-off-vertical-tabs", args: null },
      "Context menu item indicates that it disables vertical tabs."
    );
    toggleMenuItem.click();
  });
  await waitForTabstripOrientation("horizontal");
  await TestUtils.waitForCondition(
    () => !gBrowser.tabContainer.verticalMode,
    "Vertical tabs are disabled."
  );
  Assert.equal(
    Services.prefs.getStringPref("sidebar.visibility"),
    "hide-sidebar",
    "Sanity check the visibilty pref when verticalTabs are disabled"
  );

  window.SidebarController.hide();
  await SpecialPowers.popPrefEnv();
  await window.SidebarController.waitUntilStable();
});
