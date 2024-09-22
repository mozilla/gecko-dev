/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_SIDEBAR() {
  ok(!SidebarController.isOpen, "Sidebar initially closed");

  await SMATestUtils.executeAndValidateAction({
    type: "OPEN_SIDEBAR",
    data: "viewHistorySidebar",
  });

  ok(SidebarController.isOpen, "Now open");

  SidebarController.hide();
});
