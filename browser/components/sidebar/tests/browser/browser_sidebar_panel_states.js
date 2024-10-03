/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that when enabling the revamped sidebar while a sidebar is already open, the
 * current sidebar panel opens
 */
add_task(async function test_enable_revamp_with_open_sidebar() {
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", false]] });

  // lets keep sidebar state changes in a temp. new window
  // so we don't potentially pollute any results in subsequent tests
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const { document, SidebarController } = newWin;
  const sidebarLauncher = document.getElementById("sidebar-main");
  const sidebarHeader = document.getElementById("sidebar-header");

  if (SidebarController.currentID !== "viewHistorySidebar") {
    await SidebarController.show("viewHistorySidebar");
  }
  Assert.ok(SidebarController.isOpen, "isOpen is true");
  Assert.ok(sidebarLauncher.hidden, "The sidebar launcher is hidden");
  Assert.ok(!sidebarHeader.hidden, "The sidebar header is visible");

  // the new sidebar launcher element should be visible
  // and the switcher from the old sidebar should be hidden now
  let sidebarVisibilitiesChanged = Promise.all([
    BrowserTestUtils.waitForMutationCondition(
      sidebarLauncher,
      { attributes: true, attributeFilter: ["hidden"] },
      () => !sidebarLauncher.hidden
    ),
    BrowserTestUtils.waitForMutationCondition(
      sidebarHeader,
      { attributes: true, attributeFilter: ["hidden"] },
      () => sidebarHeader.hidden
    ),
  ]);

  // restore the revamp pref to true
  await SpecialPowers.popPrefEnv();
  await sidebarVisibilitiesChanged;

  Assert.ok(!sidebarLauncher.hidden, "The sidebar launcher is visible");
  Assert.ok(sidebarHeader.hidden, "The sidebar header is hidden");

  await BrowserTestUtils.closeWindow(newWin);
});
