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

const getAlertsRenderedPromise = megalist => {
  return BrowserTestUtils.waitForMutationCondition(
    megalist,
    { childList: true, subtree: true },
    async () => {
      const passwordsList = megalist.querySelector(".passwords-list");
      const logins = await Services.logins.getAllLogins();
      const breaches =
        await LoginBreaches.getPotentialBreachesByLoginGUID(logins);
      return (
        passwordsList?.querySelectorAll("password-card").length ===
        breaches.size
      );
    }
  );
};

const waitForAlertsButton = megalist => {
  return BrowserTestUtils.waitForMutationCondition(
    megalist,
    { childList: true, subtree: true },
    () => {
      const alertsButton = megalist.querySelector("#alerts");
      return alertsButton !== null;
    }
  );
};

async function checkSearchResults(expectedCount, megalist) {
  return await BrowserTestUtils.waitForCondition(() => {
    const passwordsList = megalist.querySelector(".passwords-list");
    return (
      passwordsList?.querySelectorAll("password-card").length === expectedCount
    );
  }, "Search count incorrect");
}

add_task(async function test_filter_passwords() {
  await addBreach();
  await addMockPasswords();
  info("Adding breached login");
  await Services.logins.addLoginAsync(BREACHED_LOGIN);
  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);
  info("Toggle showing only alerts");
  const alertsRenderedPromise = getAlertsRenderedPromise(megalist);

  // Ensure the alerts button is rendered.
  await waitForAlertsButton(megalist);
  const alertsButton = megalist.querySelector("#alerts");
  alertsButton.click();
  await alertsRenderedPromise;
  ok(true, "Passwords list is showing only login alerts.");

  info("Toggle showing all logins");
  const allLoginsButton = megalist.querySelector("#allLogins");
  allLoginsButton.click();
  await checkAllLoginsRendered(megalist);

  LoginTestUtils.clearData();
  SidebarController.hide();
});

add_task(async function test_filter_passwords_after_sidebar_closed() {
  const megalist = await openPasswordsSidebar();
  await addBreach();
  await addMockPasswords();
  info("Adding breached login");
  await Services.logins.addLoginAsync(BREACHED_LOGIN);
  await checkAllLoginsRendered(megalist);
  info("Toggle showing only alerts");
  const alertsRenderedPromise = getAlertsRenderedPromise(megalist);

  // Ensure the alerts button is rendered.
  await waitForAlertsButton(megalist);
  const alertsButton = megalist.querySelector("#alerts");
  alertsButton.click();
  await alertsRenderedPromise;
  info("Passwords list is showing only login alerts.");

  // Make sure that we show all saved logins after openning and closing the sidebar.
  info("Hide sidebar.");
  SidebarController.hide();
  info("Show sidebar.");
  await SidebarController.show("viewMegalistSidebar");
  await checkAllLoginsRendered(megalist);
  info("All saved logins are displayed.");

  LoginTestUtils.clearData();
  SidebarController.hide();
});

add_task(async function test_filter_passwords_while_editing() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    ok(true, "Cannot test OSAuth.");
    return;
  }

  const megalist = await openPasswordsSidebar();
  await addMockPasswords();
  await checkAllLoginsRendered(megalist);

  info("Filter password using search input");
  const searchInput = megalist.querySelector(".search");
  searchInput.value = TEST_LOGIN_2.username;
  searchInput.dispatchEvent(new Event("input"));
  await checkSearchResults(1, megalist);

  info("Ensure editing login with a filter works");
  const passwordCard = megalist.querySelector("password-card");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  info("Focus the password field.");
  const loginForm = megalist.querySelector("login-form");
  const passwordField = loginForm.shadowRoot.querySelector(
    "login-password-field"
  );
  const revealPromise = BrowserTestUtils.waitForMutationCondition(
    passwordField.input,
    {
      attributeFilter: ["type"],
    },
    () => passwordField.input.getAttribute("type") === "text"
  );
  passwordField.input.focus();
  await revealPromise;
  is(passwordField.input.value, TEST_LOGIN_2.password, "password revealed");

  const newUsername = "new_sally";
  const newPassword = "new_password_sally";
  info("Updating login.");
  setInputValue(loginForm, "login-username-field", newUsername);
  setInputValue(loginForm, "login-password-field", newPassword);

  const saveButton = loginForm.shadowRoot.querySelector(
    "moz-button[type=primary]"
  );
  info("Submitting form.");
  saveButton.buttonEl.click();

  await waitForNotification(megalist, "update-login-success");
  await checkSearchResults(1, megalist);
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
  ok(true, "Login successfully updated");

  LoginTestUtils.clearData();
  SidebarController.hide();
});

add_task(async function test_filter_passwords_and_update_login() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    ok(true, "Cannot test OSAuth");
    return;
  }
  await addMockPasswords();
  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  info("Filter password using search input");
  const searchInput = megalist.querySelector(".search");
  searchInput.value = TEST_LOGIN_3.username;
  searchInput.dispatchEvent(new Event("input"));
  await checkSearchResults(1, megalist);

  info("Ensure editing login with a filter works");
  const passwordCard = megalist.querySelector("password-card");
  await waitForReauth(() => passwordCard.editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  const newUsername = `${TEST_LOGIN_3.username}_updated`;
  const newPassword = `${TEST_LOGIN_3.password}_updated`;
  const loginForm = megalist.querySelector("login-form");

  info("Updating login.");
  setInputValue(loginForm, "login-username-field", newUsername);
  setInputValue(loginForm, "login-password-field", newPassword);
  const saveButton = loginForm.shadowRoot.querySelector(
    "moz-button[type=primary]"
  );
  info("Submitting form");
  saveButton.buttonEl.click();

  await waitForNotification(megalist, "update-login-success");
  const updatedPasswordCard = megalist.querySelector("password-card");
  info("Check that the login has a new username and password");
  await BrowserTestUtils.waitForCondition(
    () => updatedPasswordCard.usernameLine.value === newUsername,
    "Username not updated"
  );
  await waitForPasswordReveal(updatedPasswordCard.passwordLine);
  await BrowserTestUtils.waitForCondition(
    () => updatedPasswordCard.passwordLine.value === newPassword,
    "Password not updated"
  );

  LoginTestUtils.clearData();
  SidebarController.hide();
});
