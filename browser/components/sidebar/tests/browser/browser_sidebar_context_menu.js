/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_sidebar_extension_context_menu() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await waitForBrowserWindowActive(win);
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  await sidebar.updateComplete;
  ok(sidebar, "Sidebar is shown.");

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

  //   Click anywhere in the sidebar
  EventUtils.synthesizeMouseAtCenter(
    sidebar,
    { type: "contextmenu", button: 2 },
    win
  );
  is(
    contextMenu.state,
    "closed",
    "Context menu hidden when anything other than button is right clicked"
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
  await BrowserTestUtils.closeWindow(win);
});
