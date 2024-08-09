/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let btpGracePeriodSec = Services.prefs.getIntPref(
  "privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec"
);

/**
 * Registers a console listener and waits for the bounce tracker classified message
 * to be logged.
 * @returns {Promise} - Promise which resolves once the message has been logged.
 */
async function waitForBounceTrackerClassifiedMessage(siteHost) {
  let lastMessage;
  // Checking if the grace period is
  // privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec requires
  // extra steps because l10n applies further transformations to the number such
  // as adding a ",".
  let gracePeriodFormatted = new Intl.NumberFormat("en-US", {
    useGrouping: true,
  }).format(
    Services.prefs.getIntPref(
      "privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec"
    )
  );
  let isBTPClassifiedMsg = msg =>
    msg.includes(
      `“${siteHost}” has been classified as a bounce tracker. If it does not receive user activation within the next ${gracePeriodFormatted} seconds it will have its state purged.`
    );
  await new Promise(resolve => {
    SpecialPowers.registerConsoleListener(consoleMsg => {
      if (!consoleMsg?.message || consoleMsg.message == "SENTINEL") {
        return;
      }
      lastMessage = consoleMsg;
      if (isBTPClassifiedMsg(consoleMsg.message)) {
        resolve();
      }
    });
  });
  SpecialPowers.postConsoleSentinel();

  ok(lastMessage.isScriptError, "Message should be script error.");
  ok(lastMessage.isWarning, "Message should be a warning.");

  ok(
    isBTPClassifiedMsg(lastMessage.message),
    "Observed bounce tracker classified console message."
  );
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

add_task(async function test_bounce_tracker_classified_web_console_message() {
  let consoleMsgPromise = waitForBounceTrackerClassifiedMessage(SITE_TRACKER);

  info("Run server bounce with cookie.");
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
  });

  await consoleMsgPromise;
});
