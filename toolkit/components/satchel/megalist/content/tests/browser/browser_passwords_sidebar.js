/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.megalist.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });

  registerCleanupFunction(function () {
    LoginTestUtils.clearData();
  });
});

add_task(async function test_passwords_sidebar() {
  info("Add passwords");
  await LoginTestUtils.addLogin({ username: "bob", password: "pass1" });
  await LoginTestUtils.addLogin({ username: "sally", password: "pass2" });
  await LoginTestUtils.addLogin({ username: "ned", password: "pass3" });

  info("Open Passwords sidebar");
  await SidebarController.show("viewMegalistSidebar");
  const sidebar = document.getElementById("sidebar");
  const megalist =
    sidebar.contentDocument.querySelector("megalist-alpha").shadowRoot;

  info("Check that records are rendered");
  await BrowserTestUtils.waitForMutationCondition(
    megalist,
    { childList: true, subtree: true },
    async () =>
      !!megalist.querySelector(".passwords-list")?.children.length === 3
  );

  ok(true, "3 records are rendered.");
});
