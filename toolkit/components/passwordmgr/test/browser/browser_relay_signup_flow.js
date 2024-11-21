const TEST_URL_PATH = `https://example.org${DIRECTORY_PATH}form_basic_signup.html`;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/passwordmgr/test/browser/browser_relay_utils.js",
  this
);

add_task(
  async function test_default_does_not_display_Relay_to_unauthenticated_browser() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function (browser) {
        const popup = document.getElementById("PopupAutoComplete");
        await openACPopup(popup, browser, "#form-basic-username");

        const relayItem = getRelayItemFromACPopup(popup);
        Assert.ok(
          !relayItem,
          "Relay item SHOULD NOT be present in the autocomplete popup when the browser IS NOT signed in."
        );
      }
    );
  }
);

add_task(async function test_default_displays_Relay_to_signed_in_browser() {
  const sandbox = stubFxAccountsToSimulateSignedIn();
  const rsSandbox = await stubRemoteSettingsAllowList();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      const popup = document.getElementById("PopupAutoComplete");
      await openACPopup(popup, browser, "#form-basic-username");

      const relayItem = getRelayItemFromACPopup(popup);
      Assert.ok(
        relayItem,
        "Relay item SHOULD be present in the autocomplete popup when the browser IS signed in."
      );
    }
  );

  sandbox.restore();
  rsSandbox.restore();
});

add_task(
  async function test_site_not_on_allowList_still_shows_Relay_to_signed_in_browser() {
    const sandbox = stubFxAccountsToSimulateSignedIn();
    const rsSandbox = await stubRemoteSettingsAllowList([
      { domain: "not-example.org" },
    ]);
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function (browser) {
        const popup = document.getElementById("PopupAutoComplete");
        await openACPopup(popup, browser, "#form-basic-username");

        const relayItem = getRelayItemFromACPopup(popup);
        Assert.ok(
          relayItem,
          "Relay item SHOULD be present in the autocomplete popup when the site is not on the allow-list, if the user is signed into the browser."
        );
      }
    );
    sandbox.restore();
    rsSandbox.restore();
  }
);

add_task(
  async function test_authenticated_browser_use_email_mask_calls_fxa_and_relay_functions() {
    const sandbox = stubFxAccountsToSimulateSignedIn();
    const rsSandbox = await stubRemoteSettingsAllowList();
    await setUpMockRelayServer();
    setupServerScenario();

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function (browser) {
        const acPopup = document.getElementById("PopupAutoComplete");
        const notificationPopup = document.getElementById("notification-popup");

        await openACPopup(acPopup, browser, "#form-basic-username");
        await clickRelayItemAndWaitForPopup(acPopup);

        const primaryButton = notificationPopup.querySelector(
          "button.popup-notification-primary-button"
        );

        await clickButtonAndWaitForPopupToClose(primaryButton);

        await verifyConfirmationHint(
          browser,
          true,
          "identity-icon-box",
          "confirmation-hint-firefox-relay-mask-created"
        );

        // TODO: asssert the form-basic-username field contains a mask
      }
    );
    sandbox.restore();
    rsSandbox.restore();
  }
);
