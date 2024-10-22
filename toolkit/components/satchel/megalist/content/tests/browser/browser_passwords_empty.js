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

add_task(async function test_passwords_empty_state() {
  info("Check that no-login-card is shown when user has no logins.");
  const megalist = await openPasswordsSidebar();
  await checkEmptyState(".no-logins-card-content", megalist);
  ok(true, "Empty state rendered.");

  info("Test empty search results");
  await addMockPasswords();
  await checkAllLoginsRendered(megalist);
  const searchInput = megalist.querySelector(".search");
  searchInput.value = "hello";
  searchInput.dispatchEvent(new Event("input"));
  await checkEmptyState(".empty-search-results", megalist);
  ok(true, "Empty search results rendered.");

  info("Test no-login-card is shown when all logins are removed.");
  LoginTestUtils.clearData();
  await checkEmptyState(".no-logins-card-content", megalist);
  ok(true, "Empty state rendered after logins are removed.");

  info("Closing the sidebar");
  SidebarController.hide();
});

async function checkEmptyState(selector, megalist) {
  return await BrowserTestUtils.waitForCondition(() => {
    const emptyStateCard = megalist.querySelector(".empty-state-card");
    return !!emptyStateCard?.querySelector(selector);
  }, "Empty state card failed to render");
}
