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

function waitForPasswordReveal(passwordLine) {
  const revealBtnPromise = BrowserTestUtils.waitForMutationCondition(
    passwordLine.loginLine,
    {
      attributeFilter: ["inputtype"],
    },
    () => passwordLine.loginLine.getAttribute("inputtype") === "text"
  );
  info("click on reveal button");
  passwordLine.revealBtn.click();
  return revealBtnPromise;
}

add_task(async function test_update_login_success() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    ok(true, "Cannot test OSAuth.");
    return;
  }

  const login = TEST_LOGIN_1;
  await LoginTestUtils.addLogin(login);

  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCard = megalist.querySelector("password-card");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  const newUsername = "new_username";
  const newPassword = "new_password";
  const loginForm = megalist.querySelector("login-form");
  info("Updating login.");
  setInputValue(loginForm, "login-username-field", newUsername);
  setInputValue(loginForm, "login-password-field", newPassword);

  const saveButton = loginForm.shadowRoot.querySelector(
    "moz-button[type=primary]"
  );
  info("Submitting form.");
  saveButton.buttonEl.click();

  await waitForNotification(megalist, "update-login-success");
  await checkAllLoginsRendered(megalist);
  const updatedPasswordCard = megalist.querySelector("password-card");

  await BrowserTestUtils.waitForCondition(
    () => updatedPasswordCard.usernameLine.value === newUsername,
    "Username not updated."
  );
  await waitForPasswordReveal(updatedPasswordCard.passwordLine);
  await BrowserTestUtils.waitForCondition(
    () => updatedPasswordCard.passwordLine.value === newPassword,
    "Password not updated."
  );
  LoginTestUtils.clearData();
  SidebarController.hide();
});

add_task(async function test_update_login_duplicate() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    ok(true, "Cannot test OSAuth.");
    return;
  }

  const sameOriginLogins = [
    TEST_LOGIN_1,
    { ...TEST_LOGIN_2, origin: TEST_LOGIN_1.origin },
  ];

  for (const login of sameOriginLogins) {
    await LoginTestUtils.addLogin(login);
  }

  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCard = megalist.querySelector("password-card");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  const loginForm = megalist.querySelector("login-form");
  info(`updating login 1's username to login 2's username`);
  setInputValue(loginForm, "login-username-field", TEST_LOGIN_2.username);

  const saveButton = loginForm.shadowRoot.querySelector(
    "moz-button[type=primary]"
  );
  info("Submitting form.");
  saveButton.buttonEl.click();

  await waitForNotification(megalist, "login-already-exists-warning");
  LoginTestUtils.clearData();
  SidebarController.hide();
});

add_task(async function test_update_login_discard_changes() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    ok(true, "Cannot test OSAuth.");
    return;
  }

  const login = TEST_LOGIN_1;
  await LoginTestUtils.addLogin(login);

  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCard = megalist.querySelector("password-card");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  info("Cancelling form.");
  const loginForm = megalist.querySelector("login-form");
  const cancelButton = loginForm.shadowRoot.querySelector(
    "moz-button[data-l10n-id=login-item-cancel-button]"
  );
  cancelButton.buttonEl.click();
  await waitForNotification(megalist, "discard-changes");
  ok(true, "Got discard changes notification");

  info("Pressing Go Back action on notification");
  let notificationMsgBar = megalist.querySelector("notification-message-bar");
  const goBackActionButton = notificationMsgBar.shadowRoot.querySelector(
    "moz-button[type=default]"
  );
  goBackActionButton.click();
  await BrowserTestUtils.waitForCondition(() => {
    const notificationMsgBar = megalist.querySelector(
      "notification-message-bar"
    );
    return !notificationMsgBar;
  }, "Notification was not dismissed.");
  ok(true, "Discard changes notification dismissed.");

  info("Cancelling form again.");
  cancelButton.buttonEl.click();
  await waitForNotification(megalist, "discard-changes");
  ok(true, "Got discard changes notification");

  info("Pressing Confirm action on notification");
  notificationMsgBar = megalist.querySelector("notification-message-bar");
  let confirmButton = notificationMsgBar.shadowRoot.querySelector(
    "moz-button[type=destructive]"
  );
  confirmButton.click();
  await checkAllLoginsRendered(megalist);
  ok(true, "List view of logins is shown again");

  info("Try closing sidebar while editing a login");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );
  SidebarController.hide();
  await waitForNotification(megalist, "discard-changes");
  ok(true, "Got discard changes notification when closing sidebar");

  info("Sidebar should close if discard changes is confirmed");
  notificationMsgBar = megalist.querySelector("notification-message-bar");
  confirmButton = notificationMsgBar.shadowRoot.querySelector(
    "moz-button[type=destructive]"
  );
  confirmButton.click();
  await BrowserTestUtils.waitForCondition(() => {
    return !SidebarController.isOpen;
  }, "Sidebar did not close.");
  ok(!SidebarController.isOpen, "Sidebar closed");

  LoginTestUtils.clearData();
});
