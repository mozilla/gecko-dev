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

const getShadowBtn = (el, selector) =>
  el.querySelector(selector).shadowRoot.querySelector("button");

async function openLoginForm(megalist, isFromMenuDropdown = true) {
  info("Opening login form.");

  let button = megalist.querySelector(".empty-state-add-password");
  if (isFromMenuDropdown) {
    const menu = megalist.querySelector("panel-list");
    const menuButton = megalist.querySelector("#more-options-menubutton");
    menuButton.click();
    await BrowserTestUtils.waitForEvent(menu, "shown");
    button = getShadowBtn(menu, "[action='add-password']");
  }

  const loginFormPromise = BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to load."
  );
  button.click();
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

function waitForRecords(count) {
  info(`Wait for records to reach expected amount ${count}.`);
  const sidebar = document.getElementById("sidebar");
  const megalistComponent =
    sidebar.contentDocument.querySelector("megalist-alpha");
  return BrowserTestUtils.waitForCondition(
    () => megalistComponent.records.length == count,
    `records did not he ${count} elements`
  );
}

function getScrollPromise(megalist) {
  const scrollingElement = megalist.ownerDocument.scrollingElement;
  const scrollPromise = BrowserTestUtils.waitForCondition(
    () => scrollingElement.scrollTopMax == scrollingElement.scrollTop,
    "Did not scroll to new login."
  );
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
  const notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "add-login-success"
  );

  await checkAllLoginsRendered(megalist);

  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "add_login_success",
    action_type: "nav_record",
  });

  let updateEvents = Glean.contextualManager.recordsUpdate.testGetValue();
  Assert.equal(updateEvents.length, 1, "Recorded manual add password once.");
  assertCPMGleanEvent(updateEvents[0], {
    change_type: "add",
  });

  LoginTestUtils.clearData();
});

add_task(async function test_add_duplicate_login() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  await addMockPasswords();

  const megalist = await openPasswordsSidebar();
  await waitForRecords(3);
  await openLoginForm(megalist);
  addLogin(megalist, TEST_LOGIN_1);
  const notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "login-already-exists-warning"
  );
  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "login_already_exists_warning",
    action_type: "nav_record",
  });

  LoginTestUtils.clearData();
});

add_task(async function test_add_login_empty_origin() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

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
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();
  await addMockPasswords();

  const megalist = await openPasswordsSidebar();
  await waitForSnapshots();
  await openLoginForm(megalist);
  addLogin(megalist, {
    ...TEST_LOGIN_1,
    origin: "https://zzz.com",
  });

  const notifMsgBar = await checkNotificationAndTelemetry(
    megalist,
    "add-login-success"
  );
  await checkAllLoginsRendered(megalist);
  const scrollPromise = getScrollPromise(megalist);
  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "add_login_success",
    action_type: "nav_record",
  });
  await scrollPromise;
  LoginTestUtils.clearData();
});

add_task(async function test_passwords_add_password_empty_state() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const megalist = await openPasswordsSidebar();
  await checkEmptyState(".no-logins-card-content", megalist);
  ok(true, "Empty state rendered.");

  info("Add a password via empty state");
  await openLoginForm(megalist, false);
  let events = Glean.contextualManager.toolbarAction.testGetValue();
  assertCPMGleanEvent(events[0], {
    trigger: "empty_state_card",
    option_name: "add_new",
  });
  addLogin(megalist, TEST_LOGIN_1);
  const notifMsgBar = await waitForNotification(megalist, "add-login-success");
  await checkAllLoginsRendered(megalist);
  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "add_login_success",
    action_type: "nav_record",
  });

  LoginTestUtils.clearData();

  info("Closing the sidebar");
  SidebarController.hide();
});
