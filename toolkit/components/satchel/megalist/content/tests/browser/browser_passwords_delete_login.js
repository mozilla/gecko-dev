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

add_task(async function test_delete_login_success() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    ok(true, "Cannot test OSAuth.");
    return;
  }

  await addMockPasswords();
  info("Three logins added in total.");

  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCard = megalist.querySelector("password-card");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  const loginForm = megalist.querySelector("login-form").shadowRoot;
  const deleteLoginBtn = loginForm.querySelector(".delete-login-button");
  deleteLoginBtn.click();

  await BrowserTestUtils.waitForCondition(
    () => loginForm.querySelector(".remove-login-card"),
    "Remove login card failed to render."
  );

  const confirmDeleteBtn = loginForm.querySelector(
    "moz-button[type=destructive]"
  );
  confirmDeleteBtn.click();
  info("Delete one login.");

  await waitForNotification(megalist, "delete-login-success");
  await checkAllLoginsRendered(megalist);

  const numPasswords = megalist.querySelectorAll("password-card").length;
  is(numPasswords, 2, "One login was successfully deleted.");
});
