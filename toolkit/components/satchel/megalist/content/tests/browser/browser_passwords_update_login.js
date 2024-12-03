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

  await openEditLoginForm(megalist, getMegalistParent(), 0);
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

  await openEditLoginForm(megalist, getMegalistParent(), 0);
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
