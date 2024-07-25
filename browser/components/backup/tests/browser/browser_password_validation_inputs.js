/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SCHEDULED_BACKUPS_ENABLED_PREF = "browser.backup.scheduled.enabled";

/**
 * Tests that password inputs are validated and that rules are displayed
 * as expected.
 */
add_task(async function password_validation() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let settings = browser.contentDocument.querySelector("backup-settings");

    await SpecialPowers.pushPrefEnv({
      set: [[SCHEDULED_BACKUPS_ENABLED_PREF, true]],
    });

    settings.backupServiceState.encryptionEnabled = true;
    await settings.requestUpdate();
    await settings.updateComplete;

    let changePasswordButton = settings.changePasswordButtonEl;
    Assert.ok(changePasswordButton, "Change password button should be found");

    changePasswordButton.click();
    await settings.updateComplete;

    let enableBackupEncryption = settings.enableBackupEncryptionEl;
    let passwordInputs = enableBackupEncryption.passwordInputsEl;
    Assert.ok(passwordInputs, "password-validation-inputs should be found");

    let passwordRules = passwordInputs.passwordRulesEl;
    Assert.ok(passwordRules, "Password rules tooltip should be found");

    let visiblePromise = BrowserTestUtils.waitForMutationCondition(
      passwordRules,
      { attributes: true, childList: true },
      () => BrowserTestUtils.isVisible(passwordRules)
    );

    let newPasswordInput = passwordInputs.inputNewPasswordEl;
    newPasswordInput.focus();
    await visiblePromise;

    Assert.ok(true, "Password rules tooltip should be visible");

    /**
     * Even if `minlength` is not met, the input's validity state is still considered "valid"
     * as long as there's no user interaction. There are 2 ways to circumvent this limitation:
     *  1) We can enter an empty string to trigger ValidityState.valueMissing (thanks to the `required` attribute),
     *     which we also consider as too short.
     *  2) We can stub out ValidityState.tooShort and set it to true.
     */
    await createMockPassInputEventPromise(newPasswordInput, "");
    await passwordInputs.updateComplete;

    Assert.ok(
      passwordInputs._tooShort,
      "Too short password rule was detected because of an empty string"
    );
    Assert.ok(
      !passwordInputs._passwordsValid,
      "Passwords are considered invalid"
    );

    // Now pretend that we entered a password that's too short via this stub
    let validityStub = sandbox.stub(newPasswordInput, "validity").get(() => {
      return {
        tooShort: true,
        valid: false,
      };
    });
    await passwordInputs.updateComplete;

    Assert.ok(
      passwordInputs._tooShort,
      "Too short password rule was detected because of ValidityState.tooShort"
    );
    Assert.ok(
      !passwordInputs._passwordsValid,
      "Passwords are still considered invalid"
    );

    validityStub.restore();

    // Now assume an email was entered
    const mockEmail = "email@example.com";
    await createMockPassInputEventPromise(newPasswordInput, mockEmail);
    await passwordInputs.updateComplete;

    Assert.ok(
      !passwordInputs._tooShort,
      "Too short rule is no longer detected"
    );
    Assert.ok(passwordInputs._hasEmail, "Has email rule was detected");
    Assert.ok(
      !passwordInputs._passwordsValid,
      "Passwords are still considered invalid"
    );

    // Verify passwords are still invalid if they don't match
    const noMatchPass = `${MOCK_PASSWORD}-notMatch`;
    await createMockPassInputEventPromise(newPasswordInput, MOCK_PASSWORD);
    let repeatPasswordInput = passwordInputs.inputRepeatPasswordEl;
    await createMockPassInputEventPromise(repeatPasswordInput, noMatchPass);
    await passwordInputs.updateComplete;

    Assert.ok(
      !passwordInputs._hasEmail,
      "Has email rule is no longer detected"
    );
    Assert.ok(!passwordInputs._passwordsMatch, "Passwords do not match");
    Assert.ok(
      !passwordInputs._passwordsValid,
      "Passwords are still considered invalid"
    );

    // Finally, match the passwords and ensure they are valid
    await createMockPassInputEventPromise(repeatPasswordInput, MOCK_PASSWORD);
    await passwordInputs.updateComplete;

    Assert.ok(passwordInputs._passwordsMatch, "Passwords now match");
    Assert.ok(
      passwordInputs._passwordsValid,
      "Passwords are now considered valid"
    );

    let classChangePromise = BrowserTestUtils.waitForMutationCondition(
      passwordRules,
      { attributes: true, attributesFilter: ["class"] },
      () => passwordRules.classList.contains("hidden")
    );

    /*
     * We can't use waitForMutationCondition because mutation observers do not detect computed style updates following a modified class name.
     * Plus, visibility changes are delayed due to transitions. Use waitForCondition instead to wait for the animation to finish and
     * validate the tooltip's final visibility state.
     */
    let hiddenPromise = BrowserTestUtils.waitForCondition(() => {
      return BrowserTestUtils.isHidden(passwordRules);
    });

    newPasswordInput.blur();
    await passwordInputs.updateComplete;
    await classChangePromise;
    await hiddenPromise;

    Assert.ok(true, "Password rules tooltip should be hidden");
    sandbox.restore();
  });
});
