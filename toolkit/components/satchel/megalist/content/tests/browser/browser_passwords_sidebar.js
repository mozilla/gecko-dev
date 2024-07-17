/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
  await addMockPasswords();

  info("Open Passwords sidebar");
  await SidebarController.show("viewMegalistSidebar");
  const sidebar = document.getElementById("sidebar");
  const megalist =
    sidebar.contentDocument.querySelector("megalist-alpha").shadowRoot;

  info("Check that records are rendered");
  await BrowserTestUtils.waitForMutationCondition(
    megalist,
    { childList: true, subtree: true },
    async () => {
      const passwordsList = megalist.querySelector(".passwords-list");
      return passwordsList?.querySelectorAll("password-card").length === 3;
    }
  );
  ok(true, "3 password cards are rendered.");
});
