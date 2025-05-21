/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kNotificationSelector =
  'notification-message[message-bar-type="infobar"]' +
  '[value="sandbox-unprivileged-namespaces"]';

function closeExistingNotification() {
  const notification = document.querySelector(kNotificationSelector);
  if (notification) {
    notification.remove();
  }
  Assert.equal(
    null,
    document.querySelector(kNotificationSelector),
    "No more notification"
  );
}

function setHasUsernamespaces(isPresent) {
  Services.sysinfo
    .QueryInterface(Ci.nsIWritablePropertyBag2)
    .setPropertyAsBool("hasUserNamespaces", isPresent);
}

async function getNotification(shouldBeNull = false) {
  await TestUtils.waitForCondition(() => {
    if (shouldBeNull) {
      return document.querySelector(kNotificationSelector) === null;
    }
    return document.querySelector(kNotificationSelector) !== null;
  }, "Trying to get a notification");
  return document.querySelector(kNotificationSelector);
}

if (AppConstants.platform === "linux" && AppConstants.MOZ_SANDBOX) {
  let { SandboxUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/SandboxUtils.sys.mjs"
  );
  add_setup(async function setup() {
    await SpecialPowers.pushPrefEnv({
      set: [["security.sandbox.warn_unprivileged_namespaces", true]],
    });
    closeExistingNotification();
    const originalValue = Services.sysinfo.getProperty("hasUserNamespaces");
    registerCleanupFunction(() => {
      Services.sysinfo
        .QueryInterface(Ci.nsIWritablePropertyBag2)
        .setPropertyAsBool("hasUserNamespaces", originalValue);
    });
  });

  add_task(async function doNotShowNotificationCorrectly() {
    Assert.equal(
      null,
      document.querySelector(kNotificationSelector),
      "No existing notification"
    );
    setHasUsernamespaces(true);
    SandboxUtils.maybeWarnAboutMissingUserNamespaces(window);

    const notification = await getNotification(/* shouldBeNull */ true);
    Assert.equal(
      null,
      notification,
      "Notification is not shown when the feature is supported"
    );
  });

  add_task(async function showNotificationCorrectly() {
    Assert.equal(
      null,
      document.querySelector(kNotificationSelector),
      "No existing notification"
    );
    setHasUsernamespaces(false);
    SandboxUtils.maybeWarnAboutMissingUserNamespaces(window);

    const notification = await getNotification();
    Assert.notEqual(
      null,
      notification,
      "Notification is shown when the feature is not supported"
    );
    closeExistingNotification();
  });

  add_task(async function prefDisablesNotification() {
    Assert.equal(
      null,
      document.querySelector(kNotificationSelector),
      "No existing notification"
    );
    await SpecialPowers.pushPrefEnv({
      set: [["security.sandbox.warn_unprivileged_namespaces", false]],
    });
    setHasUsernamespaces(false);
    SandboxUtils.maybeWarnAboutMissingUserNamespaces(window);

    const notification = await getNotification(/* shouldBeNull */ true);
    Assert.equal(
      null,
      notification,
      "Notification is not shown when the feature is unsupported but pref disabled"
    );
  });

  add_task(async function dontShowAgainTogglePref() {
    Assert.equal(
      null,
      document.querySelector(kNotificationSelector),
      "No existing notification"
    );
    await SpecialPowers.pushPrefEnv({
      set: [["security.sandbox.warn_unprivileged_namespaces", true]],
    });

    Assert.equal(
      Services.prefs.getBoolPref(
        "security.sandbox.warn_unprivileged_namespaces"
      ),
      true,
      "Pref is enabled"
    );
    setHasUsernamespaces(false);
    SandboxUtils.maybeWarnAboutMissingUserNamespaces(window);

    const notification = await getNotification();
    const dontShowAgain = notification.querySelector(".notification-button");
    Assert.notEqual(null, dontShowAgain, "Found dismiss for ever button");

    dontShowAgain.click();
    Assert.equal(
      Services.prefs.getBoolPref(
        "security.sandbox.warn_unprivileged_namespaces"
      ),
      false,
      "Pref is disabled"
    );
  });
} else {
  add_task(async function doNotShowNotificationCorrectly() {
    Assert.equal(
      null,
      document.querySelector(kNotificationSelector),
      "No existing notification"
    );
    await Assert.rejects(
      fetch("resource://gre/modules/SandboxUtils.sys.mjs"),
      /NetworkError when attempting to fetch/,
      "SandboxUtils should not be packaged."
    );
  });
}
