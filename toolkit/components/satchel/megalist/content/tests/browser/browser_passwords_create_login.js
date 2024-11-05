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

async function openLoginForm(megalist) {
  info("Opening login form.");
  const addButton = megalist.querySelector(".first-row > moz-button");
  const loginFormPromise = BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to load."
  );
  addButton.buttonEl.click();
  return loginFormPromise;
}

function setInputValue(loginForm, fieldElement, value) {
  info(`Filling ${fieldElement} with value '${value}'.`);
  const field = loginForm.shadowRoot.querySelector(fieldElement);
  field.input.value = value;
  field.input.dispatchEvent(
    new InputEvent("input", {
      composed: true,
      bubbles: true,
    })
  );
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

function waitForNotification(megalist, notificationId) {
  info(`Wait for notification with id ${notificationId}.`);
  const notifcationPromise = BrowserTestUtils.waitForCondition(() => {
    const notificationMsgBar = megalist.querySelector(
      "notification-message-bar"
    );
    return notificationMsgBar?.notification.id === notificationId;
  }, `Notification with id ${notificationId} did not render.`);
  return notifcationPromise;
}

function waitForSnapshots() {
  info("Wait for headers.");
  const sidebar = document.getElementById("sidebar");
  const megalistComponent =
    sidebar.contentDocument.querySelector("megalist-alpha");
  return BrowserTestUtils.waitForCondition(
    () => megalistComponent.header,
    "Megalist header loaded."
  );
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
  const megalist = await openPasswordsSidebar();
  await waitForSnapshots();
  await openLoginForm(megalist);
  addLogin(megalist, TEST_LOGIN_1);
  await waitForNotification(megalist, "add-login-success");
  await checkAllLoginsRendered(megalist);

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
  await waitForNotification(megalist, "add-login-already-exists-warning");

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
