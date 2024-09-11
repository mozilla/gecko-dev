/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contextual-password-manager.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });
  registerCleanupFunction(LoginTestUtils.clearData);
});

add_task(async function test_filter_passwords() {
  await addBreach();
  await addMockPasswords();
  info("Adding breached login");
  await Services.logins.addLoginAsync(BREACHED_LOGIN);
  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  info("Toggle showing only alerts");
  const alertsRenderedPromise = BrowserTestUtils.waitForMutationCondition(
    megalist,
    { childList: true, subtree: true },
    async () => {
      const passwordsList = megalist.querySelector(".passwords-list");
      const logins = await Services.logins.getAllLogins();
      const breaches = await LoginBreaches.getPotentialBreachesByLoginGUID(
        logins
      );
      return (
        passwordsList?.querySelectorAll("password-card").length ===
        breaches.size
      );
    }
  );
  // Ensure the alerts button is rendered.
  await BrowserTestUtils.waitForMutationCondition(
    megalist,
    { childList: true, subtree: true },
    () => {
      const alertsButton = megalist.querySelector("#alerts");
      return alertsButton !== null;
    }
  );
  const alertsButton = megalist.querySelector("#alerts");
  alertsButton.click();
  await alertsRenderedPromise;
  ok(true, "Passwords list is showing only login alerts.");

  info("Toggle showing all logins");
  const allLoginsButton = megalist.querySelector("#allLogins");
  allLoginsButton.click();
  await checkAllLoginsRendered(megalist);
  SidebarController.hide();
});
