/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contextual-password-manager.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });
  registerCleanupFunction(() => {
    LoginTestUtils.clearData();
    LoginTestUtils.primaryPassword.disable();
  });
});

/**
 * Waits for the primary password prompt and performs an action.
 *
 * @param {string} action Set to "authenticate" to log in or "cancel" to
 *        close the dialog without logging in.
 * @param {number} timeout The amount of time in ms to wait for the dialog to open.
 */
async function waitForPPDialog(action, timeout, aWindow = window) {
  info(`Wait for PP dialog for ${timeout} ms and ${action}.`);
  let dialogShown = TestUtils.topicObserved("common-dialog-loaded");
  let timer;
  const timeoutPromise = new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    timer = setTimeout(() => {
      clearTimeout(timer);
      resolve("timeout");
    }, timeout);
  });

  const result = await Promise.race([timeoutPromise, dialogShown]);

  if (result == "timeout") {
    info(`Waiting for PP dialog timed out after ${timeout} ms.`);
    return Promise.reject();
  }

  const [subject] = result;
  let dialog = subject.Dialog;
  if (action == "authenticate") {
    SpecialPowers.wrap(dialog.ui.password1Textbox).setUserInput(
      LoginTestUtils.primaryPassword.primaryPassword
    );
    dialog.ui.button0.click();
  } else if (action == "cancel") {
    dialog.ui.button1.click();
  }

  if (timer) {
    clearTimeout(timer);
  }

  return BrowserTestUtils.waitForEvent(aWindow, "DOMModalDialogClosed");
}

add_task(async function test_primary_password_authentication_causes_refresh() {
  Assert.equal(
    Services.prefs.getStringPref(
      "toolkit.osKeyStore.unofficialBuildOnlyLogin",
      ""
    ),
    "",
    "Pref should be set to default value of empty string to start the test"
  );

  await addMockPasswords();
  LoginTestUtils.primaryPassword.enable();
  let ppDialogShownMegalist = waitForPPDialog("cancel", 5000);
  const megalist = await openPasswordsSidebar();

  try {
    await ppDialogShownMegalist;
  } catch {
    // After the test initially runs in the TV test suites, MegalistAlpha will not request LoginDataSource to reload
    // logins, and so the PP dialog will not appear. We need to manually notify LoginDataSource to reload logins.
    ppDialogShownMegalist = waitForPPDialog("cancel", 5000);
    info(
      "Notify LoginDataSource and try to cancel the primary password dialog again."
    );
    Services.obs.notifyObservers(null, "passwordmgr-storage-changed");
    await ppDialogShownMegalist;
  }

  info("No logins should be displayed if the user is not authenticated.");
  await checkEmptyState(".no-logins-card-content", megalist);
  const mpDialogShownAboutLogins = waitForPPDialog("authenticate", 5000);
  info("Open about:logins.");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  info("Authenticate user in aboutlogins.");
  await mpDialogShownAboutLogins;

  info(
    "Authentication in aboutlogins should cause megalist to render all logins."
  );
  await checkAllLoginsRendered(megalist);

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SidebarController.hide();
  info("Closing the sidebar");
});
