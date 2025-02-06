/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyServiceGetters(this, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);
const { Policy, TelemetryReportingPolicy } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryReportingPolicy.sys.mjs"
);
const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);

const TEST_SCREENS = [
  {
    id: "TEST_SCREEN",
    content: {
      position: "split",
      logo: {},
      title: "test",
    },
  },
];

const TEST_SCREEN_SELECTOR = `.onboardingContainer .screen.${TEST_SCREENS[0].id}`;

async function waitForClick(selector, win) {
  await TestUtils.waitForCondition(() => win.document.querySelector(selector));
  win.document.querySelector(selector).click();
}

// Fake dismissing a modal dialog.
function fakeInteractWithModal() {
  Services.obs.notifyObservers(
    null,
    "datareporting:notify-data-policy:interacted"
  );
}

async function showPreonboardingModal({
  screens = TEST_SCREENS,
  requireAction = false,
} = {}) {
  const PREFS_TO_SET = [
    ["browser.preonboarding.enabled", true],
    ["browser.preonboarding.screens", JSON.stringify(screens)],
    ["browser.preonboarding.requireAction", requireAction],
    ["browser.preonboarding.minimumPolicyVersion", 900],
    ["browser.preonboarding.currentPolicyVersion", 900],
    ["browser.startup.homepage_override.mstone", ""],
    ["startup.homepage_welcome_url", "about:welcome"],
    ["toolkit.telemetry.log.level", "trace"],
  ];
  const PREFS_TO_CLEAR = [
    [TelemetryUtils.Preferences.AcceptedPolicyDate],
    [TelemetryUtils.Preferences.AcceptedPolicyVersion],
    [TelemetryUtils.Preferences.BypassNotification],
  ];
  await SpecialPowers.pushPrefEnv({
    set: PREFS_TO_SET,
    clear: PREFS_TO_CLEAR,
  });

  // Pick up configuration from Nimbus feature.
  TelemetryReportingPolicy.reset();
  await Policy.fakeSessionRestoreNotification();

  let completed = false;
  let p = BROWSER_GLUE._maybeShowDefaultBrowserPrompt().then(
    () => (completed = true)
  );

  Assert.equal(
    completed,
    false,
    "The default browser prompt invocation waits for the user to be notified"
  );

  fakeInteractWithModal();
  await p;

  Assert.equal(
    completed,
    true,
    "The default browser prompt invocation waits for the user to be notified"
  );

  registerCleanupFunction(async () => {
    PREFS_TO_SET.forEach(pref => Services.prefs.clearUserPref(pref[0]));
    BrowserHandler.firstRunProfile = false;
  });
}

add_task(async function show_preonboarding_modal() {
  let messageSpy = sinon.spy(SpecialMessageActions, "handleAction");
  let showModalSpy = sinon.spy(Policy, "showModal");
  await showPreonboardingModal();

  sinon.assert.called(showModalSpy);

  const [win] = await TestUtils.topicObserved("subdialog-loaded");

  Assert.equal(
    Cc["@mozilla.org/browser/clh;1"]
      .getService(Ci.nsIBrowserHandler)
      .getFirstWindowArgs(),
    "about:welcome",
    "First window will be about:welcome"
  );

  // Wait for screen content to render
  await TestUtils.waitForCondition(() =>
    win.document.querySelector(TEST_SCREEN_SELECTOR)
  );

  Assert.ok(
    !!win.document.querySelector(TEST_SCREEN_SELECTOR),
    "Modal renders with custom screen"
  );

  Assert.equal(
    messageSpy.firstCall.args[0].data.content.disableEscClose,
    false,
    "Closing via ESC is not disabled"
  );

  await win.close();
  sinon.restore();
});

add_task(async function can_disable_closing_via_esc() {
  let messageSpy = sinon.spy(SpecialMessageActions, "handleAction");
  let showModalSpy = sinon.spy(Policy, "showModal");
  await showPreonboardingModal({ requireAction: true });

  sinon.assert.called(showModalSpy);

  const [win] = await TestUtils.topicObserved("subdialog-loaded");

  Assert.equal(
    Cc["@mozilla.org/browser/clh;1"]
      .getService(Ci.nsIBrowserHandler)
      .getFirstWindowArgs(),
    "about:welcome",
    "First window will be about:welcome"
  );

  // Wait for screen content to render
  await TestUtils.waitForCondition(() =>
    win.document.querySelector(TEST_SCREEN_SELECTOR)
  );

  Assert.ok(
    !!win.document.querySelector(TEST_SCREEN_SELECTOR),
    "Modal renders with custom screen"
  );

  Assert.equal(
    messageSpy.firstCall.args[0].data.content.disableEscClose,
    true,
    "Closing via ESC is disabled"
  );

  await win.close();
  sinon.restore();
});
