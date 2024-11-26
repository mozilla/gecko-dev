"use strict";

const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);

const { OSKeyStore } = ChromeUtils.importESModule(
  "resource://gre/modules/OSKeyStore.sys.mjs"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contextual-password-manager.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });
  registerCleanupFunction(LoginTestUtils.clearData);
});

function getMegalistParent() {
  const megalistChromeWindow = gBrowser.ownerGlobal[0];
  return megalistChromeWindow.browsingContext.currentWindowGlobal.getActor(
    "Megalist"
  );
}

async function openLoginForm(megalist, megalistParent) {
  const passwordCard = megalist.querySelector("password-card");
  const authExpirationTime = megalistParent.authExpirationTime();
  let reauthObserved = Promise.resolve();

  if (OSKeyStore.canReauth() && Date.now() > authExpirationTime) {
    reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  passwordCard.editBtn.click();
  await reauthObserved;
  return BrowserTestUtils.waitForCondition(
    () => megalist.querySelector("login-form"),
    "Login form failed to render"
  );
}

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
  const login = TEST_LOGIN_1;
  await LoginTestUtils.addLogin(login);

  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  await openLoginForm(megalist, getMegalistParent());
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
  const passwordCard = megalist.querySelector("password-card");

  await BrowserTestUtils.waitForCondition(
    () => passwordCard.usernameLine.value === newUsername,
    "Username not updated."
  );
  await waitForPasswordReveal(passwordCard.passwordLine);
  await BrowserTestUtils.waitForCondition(
    () => passwordCard.passwordLine.value === newPassword,
    "Password not updated."
  );
  LoginTestUtils.clearData();
});

add_task(async function test_update_login_duplicate() {
  const sameOriginLogins = [
    TEST_LOGIN_1,
    { ...TEST_LOGIN_2, origin: TEST_LOGIN_1.origin },
  ];

  for (const login of sameOriginLogins) {
    await LoginTestUtils.addLogin(login);
  }

  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  await openLoginForm(megalist, getMegalistParent());
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
});
