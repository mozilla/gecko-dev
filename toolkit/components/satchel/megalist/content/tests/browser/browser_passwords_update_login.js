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

add_task(async function test_update_login_success() {
  const canTestOSAuth = await resetTelemetryIfKeyStoreTestable();
  if (!canTestOSAuth) {
    return;
  }

  const login = TEST_LOGIN_1;
  await LoginTestUtils.addLogin(login);

  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCard = megalist.querySelector("password-card");
  await waitForReauth(() => passwordCard.editBtn.click());
  let events = Glean.contextualManager.recordsInteraction.testGetValue();
  assertCPMGleanEvent(events[0], {
    interaction_type: "edit",
  });
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

  const notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "update-login-success"
  );
  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "update_login_success",
    action_type: "dismiss",
  });
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

  let updateEvents = Glean.contextualManager.recordsUpdate.testGetValue();
  Assert.equal(updateEvents.length, 1, "Recorded update password once.");
  assertCPMGleanEvent(updateEvents[0], {
    change_type: "edit",
  });
  LoginTestUtils.clearData();
  SidebarController.hide();
});

add_task(async function test_update_login_duplicate() {
  const canTestOSAuth = await resetTelemetryIfKeyStoreTestable();
  if (!canTestOSAuth) {
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

  const notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "login-already-exists-warning"
  );
  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "login_already_exists_warning",
    action_type: "nav_record",
  });
  LoginTestUtils.clearData();
  SidebarController.hide();
});

add_task(async function test_update_login_discard_changes() {
  const canTestOSAuth = await resetTelemetryIfKeyStoreTestable();
  if (!canTestOSAuth) {
    return;
  }

  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

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

  // Only show the discard changes notification if the login form has been modified.
  setInputValue(loginForm, "login-username-field", login.username + "added");

  const cancelButton = loginForm.shadowRoot.querySelector(
    "moz-button[data-l10n-id=login-item-cancel-button]"
  );
  cancelButton.buttonEl.click();
  let notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "discard-changes"
  );
  ok(true, "Got discard changes notification");
  checkNotificationInteractionTelemetry(notifMsgBar, "secondary-action", {
    notification_detail: "discard_changes",
    action_type: "dismiss",
  });
  info("Pressing Go Back action on notification");
  await BrowserTestUtils.waitForCondition(() => {
    const notificationMsgBar = megalist.querySelector(
      "notification-message-bar"
    );
    return !notificationMsgBar;
  }, "Notification was not dismissed.");
  ok(true, "Discard changes notification dismissed.");

  info("Cancelling form again.");
  cancelButton.buttonEl.click();
  notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "discard-changes",
    1
  );
  ok(true, "Got discard changes notification");

  info("Pressing Confirm action on notification");
  checkNotificationInteractionTelemetry(
    notifMsgBar,
    "primary-action",
    {
      notification_detail: "discard_changes",
      action_type: "confirm_discard_changes",
    },
    1
  );
  await checkAllLoginsRendered(megalist);
  ok(true, "List view of logins is shown again");

  info("Try closing sidebar while editing a login");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );
  SidebarController.hide();
  notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "discard-changes",
    2
  );
  ok(true, "Got discard changes notification when closing sidebar");

  info("Sidebar should close if discard changes is confirmed");
  checkNotificationInteractionTelemetry(
    notifMsgBar,
    "primary-action",
    {
      notification_detail: "discard_changes",
      action_type: "confirm_discard_changes",
    },
    1
  );

  await BrowserTestUtils.waitForCondition(() => {
    return !SidebarController.isOpen;
  }, "Sidebar did not close.");
  ok(!SidebarController.isOpen, "Sidebar closed");

  LoginTestUtils.clearData();
});

add_task(async function test_update_login_without_changes() {
  const canTestOSAuth = await resetTelemetryIfKeyStoreTestable();
  if (!canTestOSAuth) {
    return;
  }

  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

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
  await ensureNoNotifications(megalist, "discard-changes");

  await checkAllLoginsRendered(megalist);
  ok(true, "List view of logins is shown again");

  /* TODO: Fix this in Bug 1946726
  info("Try closing sidebar while editing a login");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );
  SidebarController.hide();

  await BrowserTestUtils.waitForCondition(() => {
    return !SidebarController.isOpen;
  }, "Sidebar did not close.");
  ok(!SidebarController.isOpen, "Sidebar closed");
  */

  LoginTestUtils.clearData();
});
