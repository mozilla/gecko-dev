Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/passwordmgr/test/browser/browser_relay_utils.js",
  this
);

const TEST_URL_PATH = `https://example.org${DIRECTORY_PATH}form_basic_signup.html`;

const setupRelayScenario = async scenarioName => {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.firefoxRelay.feature", scenarioName]],
  });
  Services.telemetry.clearEvents();
};

const collectRelayTelemeryEvent = sameFlow => {
  const collectedEvents = TelemetryTestUtils.getEvents(
    { category: "relay_integration" },
    { process: "parent" }
  );

  return sameFlow
    ? collectedEvents.filter((event, _, arr) => event.value === arr[0].value)
    : collectedEvents;
};

const waitForEvents = async (expectedEvents, sameFlow) => {
  await TestUtils.waitForCondition(
    () =>
      (collectRelayTelemeryEvent(sameFlow)?.length ?? 0) >=
      (expectedEvents.length ?? 0),
    "Wait for telemetry to be collected",
    100,
    100
  );
  return collectRelayTelemeryEvent(sameFlow);
};

async function assertEvents(expectedEvents, sameFlow = true) {
  // To avoid intermittent failures, we wait for telemetry to be collected
  const events = await waitForEvents(expectedEvents, sameFlow);
  for (let i = 0; i < expectedEvents.length; i++) {
    const keysInExpectedEvent = Object.keys(expectedEvents[i]);
    keysInExpectedEvent.forEach(key => {
      const assertFn =
        typeof events[i][key] === "object"
          ? Assert.deepEqual.bind(Assert)
          : Assert.equal.bind(Assert);
      assertFn(
        events[i][key],
        expectedEvents[i][key],
        `Key value for ${key} should match`
      );
    });
  }
}

async function openRelayAC(browser) {
  // In rare cases, especially in chaos mode in verify tests, some events creep in.
  // Clear them out before we start.
  Services.telemetry.clearEvents();
  const popup = document.getElementById("PopupAutoComplete");
  await openACPopup(popup, browser, "#form-basic-username");
  const popupItem = document
    .querySelector("richlistitem")
    .getAttribute("ac-value");

  Assert.ok(
    gRelayACOptionsTitles.some(title => title.value === popupItem),
    "AC Popup has an item Relay option shown in popup"
  );

  const promiseHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  popup.firstChild.getItemAtIndex(0).click();
  await promiseHidden;
}

// Bug 1832782: On OSX opt verify mode, the test exceeds the default timeout.
requestLongerTimeout(2);

add_setup(async function () {
  await setUpMockRelayServer();

  const canRecordExtendedOld = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.telemetry.clearEvents();
  registerCleanupFunction(async () => {
    Services.telemetry.clearEvents();
    Services.telemetry.canRecordExtended = canRecordExtendedOld;
    sinon.restore();
  });

  stubFxAccountsToSimulateSignedIn();
});

add_task(async function test_pref_toggle() {
  await setupRelayScenario("available");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:preferences#privacy",
    },
    async _browser => {
      const relayIntegrationCheckbox = content.document.querySelector(
        "checkbox#relayIntegration"
      );
      relayIntegrationCheckbox.click();
      relayIntegrationCheckbox.click();
      await assertEvents([
        { object: "pref_change", method: "disabled" },
        { object: "pref_change", method: "enabled" },
      ]);
    }
  );
});

add_task(async function test_popup_option_optin_enabled() {
  await setupRelayScenario("available");
  setupServerScenario();
  const rsSandbox = await stubRemoteSettingsAllowList();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;

      notificationPopup
        .querySelector("button.popup-notification-primary-button")
        .click();

      await Promise.all([
        notificationHidden,
        BrowserTestUtils.waitForEvent(ConfirmationHint._panel, "popuphidden"),
        TestUtils.waitForPrefChange("signon.firefoxRelay.feature"),
      ]);

      await assertEvents([
        {
          object: "offer_relay",
          method: "shown",
          extra: { scenario: "SignUpFormScenario" },
        },
        {
          object: "offer_relay",
          method: "clicked",
          extra: { scenario: "SignUpFormScenario" },
        },
        { object: "opt_in_panel", method: "shown" },
        { object: "opt_in_panel", method: "enabled" },
      ]);

      Services.telemetry.clearEvents();

      // Retrigger AC popup
      await SpecialPowers.spawn(browser, [], async function () {
        const usernameInput = content.document.querySelector(
          "#form-basic-username"
        );
        usernameInput.blur();
        usernameInput.focus();
      });

      await assertEvents([
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "0" },
        },
      ]);
    }
  );
  rsSandbox.restore();
});

add_task(async function test_popup_option_optin_postponed() {
  await setupRelayScenario("available");
  const rsSandbox = await stubRemoteSettingsAllowList();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;

      notificationPopup
        .querySelector("button.popup-notification-secondary-button")
        .click();

      await notificationHidden;

      await assertEvents([
        { object: "offer_relay", method: "shown" },
        { object: "offer_relay", method: "clicked" },
        { object: "opt_in_panel", method: "shown" },
        { object: "opt_in_panel", method: "postponed" },
      ]);
    }
  );
  rsSandbox.restore();
});

add_task(async function test_popup_option_optin_disabled() {
  await setupRelayScenario("available");
  const rsSandbox = await stubRemoteSettingsAllowList();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;
      const menupopup = notificationPopup.querySelector("menupopup");
      const menuitem = menupopup.querySelector("menuitem");

      menuitem.click();
      await notificationHidden;

      await assertEvents([
        { object: "offer_relay", method: "shown" },
        { object: "offer_relay", method: "clicked" },
        { object: "opt_in_panel", method: "shown" },
        { object: "opt_in_panel", method: "disabled" },
      ]);
    }
  );
  rsSandbox.restore();
});

add_task(async function test_popup_option_fillusername() {
  await setupRelayScenario("enabled");
  const rsSandbox = await stubRemoteSettingsAllowList();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      await BrowserTestUtils.waitForEvent(
        ConfirmationHint._panel,
        "popuphidden"
      );
      await assertEvents([
        { object: "fill_username", method: "shown" },
        {
          object: "fill_username",
          method: "clicked",
        },
      ]);
    }
  );
  rsSandbox.restore();
});

add_task(async function test_fillusername_free_tier_limit() {
  await setupRelayScenario("enabled");
  setupServerScenario("free_tier_limit");
  const rsSandbox = await stubRemoteSettingsAllowList();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);

      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;
      notificationPopup.querySelector(".reusable-relay-masks button").click();
      await notificationHidden;

      await assertEvents([
        { object: "fill_username", method: "shown" },
        {
          object: "fill_username",
          method: "clicked",
        },
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "free_tier_limit" },
        },
        {
          object: "reuse_panel",
          method: "shown",
        },
        {
          object: "reuse_panel",
          method: "reuse_mask",
        },
      ]);

      await SpecialPowers.spawn(browser, [], async function () {
        const username = content.document.getElementById("form-basic-username");
        Assert.equal(
          username.value,
          "email1@mozilla.com",
          "Username field should be filled with the first mask"
        );
      });
    }
  );
  rsSandbox.restore();
});

add_task(async function test_fillusername_error() {
  await setupRelayScenario("enabled");
  setupServerScenario("unknown_error");
  const rsSandbox = await stubRemoteSettingsAllowList();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);

      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );

      await notificationShown;
      Assert.equal(
        notificationPopup.querySelector("popupnotification").id,
        "relay-integration-error-notification",
        "Error message should be displayed"
      );

      await assertEvents([
        { object: "fill_username", method: "shown" },
        {
          object: "fill_username",
          method: "clicked",
        },
        {
          object: "reuse_panel",
          method: "shown",
          extra: { error_code: "408" },
        },
      ]);
    }
  );
  rsSandbox.restore();
});

add_task(async function test_auth_token_error() {
  setupRelayScenario("enabled");
  const rsSandbox = await stubRemoteSettingsAllowList();
  gFxAccounts.getOAuthToken.restore();
  const oauthTokenStub = sinon.stub(gFxAccounts, "getOAuthToken").throws();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;

      notificationPopup
        .querySelector("button.popup-notification-primary-button")
        .click();

      await notificationHidden;

      await assertEvents([
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "0" },
        },
        {
          object: "fill_username",
          method: "clicked",
          extra: { error_code: "0" },
        },
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "418" },
        },
      ]);
    }
  );
  rsSandbox.restore();
  oauthTokenStub.restore();
});
