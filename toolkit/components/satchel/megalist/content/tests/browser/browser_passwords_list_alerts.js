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

async function close_sidebar(megalist) {
  LoginTestUtils.clearData();
  info("Closing the sidebar");
  SidebarController.hide();
  const notifMsgBar = await waitForNotification(megalist, "discard-changes");
  notifMsgBar.shadowRoot
    .querySelector("moz-button[type='destructive']")
    .click();
}

add_task(async function test_breached_origin_alert() {
  await addBreach();
  info("Adding a login with a breached origin.");
  await Services.logins.addLoginAsync(BREACHED_LOGIN);
  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCards = Array.from(megalist.querySelectorAll("password-card"));
  const breachedPasswordCard = passwordCards.find(
    passwordCard => passwordCard.origin.href === BREACHED_LOGIN.origin
  );
  const viewAlertsButton =
    breachedPasswordCard.shadowRoot.querySelector(".view-alert-button");
  info("Click on view alerts.");
  viewAlertsButton.click();
  const notifMsgBar = await waitForNotification(
    megalist,
    "breached-origin-warning"
  );

  info("Click on change password.");
  const editBtn = notifMsgBar.shadowRoot.querySelector("moz-button");
  await waitForReauth(() => editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  await close_sidebar(megalist);
});

add_task(async function test_no_username_alert() {
  info("Adding a login with no username.");
  await Services.logins.addLoginAsync({ ...TEST_LOGIN_1, username: "" });
  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCards = Array.from(megalist.querySelectorAll("password-card"));
  const breachedPasswordCard = passwordCards.find(
    passwordCard => passwordCard.origin.href === TEST_LOGIN_1.origin
  );
  const viewAlertsButton =
    breachedPasswordCard.shadowRoot.querySelector(".view-alert-button");

  info("Click on view alerts.");
  viewAlertsButton.click();
  const notifMsgBar = await waitForNotification(
    megalist,
    "no-username-warning"
  );

  info("Click on add username.");
  const editBtn = notifMsgBar.shadowRoot.querySelector("moz-button");
  await waitForReauth(() => editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  await close_sidebar(megalist);
});

add_task(async function test_vulnerable_password_alert() {
  await addBreach();
  info("Adding a login with a vulnerable password.");
  await Services.logins.addLoginAsync(BREACHED_LOGIN);
  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  const passwordCards = Array.from(megalist.querySelectorAll("password-card"));
  const breachedPasswordCard = passwordCards.find(
    passwordCard => passwordCard.origin.href === BREACHED_LOGIN.origin
  );
  const viewAlertsButton =
    breachedPasswordCard.shadowRoot.querySelector(".view-alert-button");
  info("Click on view alerts.");
  viewAlertsButton.click();
  const notifMsgBar = await waitForNotification(
    megalist,
    "vulnerable-password-warning"
  );

  info("Click on change password.");
  const editBtn = notifMsgBar.shadowRoot.querySelector("moz-button");
  await waitForReauth(() => editBtn.click());
  await BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );

  await close_sidebar(megalist);
});
