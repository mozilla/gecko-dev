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

async function openLoginForm(megalist, selector = ".first-row > moz-button") {
  info("Opening login form.");
  const addButton = megalist.querySelector(selector);
  const loginFormPromise = BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to load."
  );
  addButton.buttonEl.click();
  return loginFormPromise;
}

function addLogin(megalist, { origin, username, password }) {
  const loginForm = megalist.querySelector("login-form");
  setInputValue(loginForm, "login-origin-field", origin);
  setInputValue(loginForm, "login-username-field", username);
  setInputValue(loginForm, "login-password-field", password);
  const saveButton = loginForm.shadowRoot.querySelector(
    "moz-button[type=primary]"
  );
  info("Submitting form.");
  saveButton.buttonEl.click();
}

function waitForPopup(megalist, element) {
  info(`Wait for ${element} popup`);
  const loginForm = megalist.querySelector("login-form");
  const popupPromise = BrowserTestUtils.waitForCondition(
    () => loginForm.shadowRoot.querySelector(`${element}`),
    `${element} popup did not render.`
  );
  return popupPromise;
}

function waitForScroll(megalist) {
  const notificationMsgBar = megalist.querySelector(
    "notification-message-bar"
  ).shadowRoot;
  const scrollingElement = megalist.ownerDocument.scrollingElement;
  const scrollPromise = BrowserTestUtils.waitForCondition(
    () => scrollingElement.scrollTopMax == scrollingElement.scrollTop,
    "Did not scroll to new login."
  );
  notificationMsgBar.querySelector("moz-button").click();
  return scrollPromise;
}

add_task(async function test_add_login_success() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const megalist = await openPasswordsSidebar();
  await waitForSnapshots();
  await openLoginForm(megalist);

  let toolbarEvents = Glean.contextualManager.toolbarAction.testGetValue();
  Assert.equal(toolbarEvents.length, 1, "Recorded add password once.");
  assertCPMGleanEvent(toolbarEvents[0], {
    trigger: "toolbar",
    option_name: "add_new",
  });

  addLogin(megalist, TEST_LOGIN_1);
  await waitForNotification(megalist, "add-login-success");
  await checkAllLoginsRendered(megalist);

  let updateEvents = Glean.contextualManager.recordsUpdate.testGetValue();
  Assert.equal(updateEvents.length, 1, "Recorded manual add password once.");
  assertCPMGleanEvent(updateEvents[0], {
    change_type: "add",
  });

  LoginTestUtils.clearData();
});

add_task(async function test_add_duplicate_login() {
  const mock_passwords = [TEST_LOGIN_1, TEST_LOGIN_2, TEST_LOGIN_3];
  for (let login of mock_passwords) {
    info(`Saving login: ${login.username}, ${login.password}, ${login.origin}`);
    await LoginTestUtils.addLogin(login);
  }

  const megalist = await openPasswordsSidebar();
  await waitForSnapshots();
  await openLoginForm(megalist);
  addLogin(megalist, TEST_LOGIN_1);
  await waitForNotification(megalist, "login-already-exists-warning");

  LoginTestUtils.clearData();
});

add_task(async function test_add_login_empty_origin() {
  const megalist = await openPasswordsSidebar();
  await waitForSnapshots();
  await openLoginForm(megalist);
  addLogin(megalist, {
    ...TEST_LOGIN_1,
    origin: "",
  });
  await waitForPopup(megalist, "origin-warning");

  const logins = await Services.logins.getAllLogins();
  is(logins.length, 0, "No login was added after submitting form.");

  LoginTestUtils.clearData();
});

add_task(async function test_add_login_empty_password() {
  const megalist = await openPasswordsSidebar();
  await waitForSnapshots();
  await openLoginForm(megalist);
  addLogin(megalist, {
    ...TEST_LOGIN_1,
    password: "",
  });

  await waitForPopup(megalist, "password-warning");
  const logins = await Services.logins.getAllLogins();
  is(logins.length, 0, "No login was added after submitting form.");

  LoginTestUtils.clearData();
});

add_task(async function test_view_login_command() {
  await addMockPasswords();

  const megalist = await openPasswordsSidebar();
  await waitForSnapshots();
  await openLoginForm(megalist);
  addLogin(megalist, {
    ...TEST_LOGIN_1,
    origin: "https://zzz.com",
  });

  await waitForNotification(megalist, "add-login-success");
  await checkAllLoginsRendered(megalist);
  await waitForScroll(megalist);

  LoginTestUtils.clearData();
});

add_task(async function test_passwords_add_password_empty_state() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const megalist = await openPasswordsSidebar();
  await checkEmptyState(".no-logins-card-content", megalist);
  ok(true, "Empty state rendered.");

  info("Add a password via empty state");
  await openLoginForm(megalist, ".empty-state-add-password");
  let events = Glean.contextualManager.toolbarAction.testGetValue();
  assertCPMGleanEvent(events[0], {
    trigger: "empty_state_card",
    option_name: "add_new",
  });
  addLogin(megalist, TEST_LOGIN_1);
  await waitForNotification(megalist, "add-login-success");
  await checkAllLoginsRendered(megalist);

  LoginTestUtils.clearData();

  info("Closing the sidebar");
  SidebarController.hide();
});
