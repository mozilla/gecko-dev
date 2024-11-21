/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_remote_sidebar_browser() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["sidebar.main.tools", "reviewchecker,syncedtabs,history"],
    ],
  });
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  // Non-remote sidebar
  await SidebarController.show("viewHistorySidebar");
  ok(SidebarController.browser, "Sidebar browser is shown.");
  ok(
    !SidebarController.browser.hasAttribute("remote"),
    "Sidebar browser is not remote."
  );

  // Remote content sidebar
  await SidebarController.show("viewReviewCheckerSidebar");
  ok(SidebarController.browser, "Sidebar browser is shown.");
  Assert.equal(
    SidebarController.browser.getAttribute("remote"),
    "true",
    "Sidebar browser is remote."
  );
  Assert.equal(
    SidebarController.browser.getAttribute("type"),
    "content",
    "Sidebar browser is remote."
  );

  // Another non-remote sidebar
  await SidebarController.show("viewTabsSidebar");
  ok(SidebarController.browser, "Sidebar browser is shown.");
  ok(
    !SidebarController.browser.hasAttribute("remote"),
    "Sidebar browser is not remote."
  );

  await SpecialPowers.popPrefEnv();
});
