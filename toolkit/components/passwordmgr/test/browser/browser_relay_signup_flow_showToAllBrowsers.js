const TEST_URL_PATH = `https://example.org${DIRECTORY_PATH}form_basic_signup.html`;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/passwordmgr/test/browser/browser_relay_utils.js",
  this
);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.firefoxRelay.showToAllBrowsers", true],
      ["identity.fxaccounts.oauth.enabled", false],
      ["identity.fxaccounts.contextParam", "fx_desktop_v3"],
    ],
  });
});

add_task(
  async function test_showToAllBrowsers_displays_Relay_autocomplete_item_to_unauthenticated_browser() {
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
          "Relay item SHOULD be present in the autocomplete popup when the browser IS NOT signed in and the signon.firefoxRelay.showToAllBrowsers config is set to true."
        );
      }
    );
    rsSandbox.restore();
  }
);

add_task(async function test_site_not_on_allowList_doesnt_show_Relay() {
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
        !relayItem,
        "Relay item SHOULD NOT be present in the autocomplete popup when the site is not on the allow-list."
      );
    }
  );
  rsSandbox.restore();
});

add_task(
  async function test_showToAllBrowsers_open_ACPopup_twice_calls_RemoteSettings_once() {
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
          "Relay item SHOULD be present in the autocomplete popup when the browser IS NOT signed in and the signon.firefoxRelay.showToAllBrowsers config is set to true."
        );
      }
    );
    const rsSandboxRemoteSettingsGetCallsBeforeSecondACPopup =
      rsSandbox.getFakes()[0].callCount;
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
          "Relay item SHOULD be present in the autocomplete popup when the browser IS NOT signed in and the signon.firefoxRelay.showToAllBrowsers config is set to true."
        );
      }
    );
    Assert.equal(
      rsSandbox.getFakes()[0].callCount,
      rsSandboxRemoteSettingsGetCallsBeforeSecondACPopup,
      "FirefoxRelay onAllowList should only call RemoteSettings.get() once."
    );
    rsSandbox.restore();
  }
);

add_task(
  async function test_showToAllBrowsers_click_on_Relay_opens_optin_prompt() {
    const rsSandbox = await stubRemoteSettingsAllowList();
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function (browser) {
        const acPopup = document.getElementById("PopupAutoComplete");
        await openACPopup(acPopup, browser, "#form-basic-username");
        await clickRelayItemAndWaitForPopup(acPopup);

        const fxaRelayOptInPrompt = document.getElementById(
          "fxa-and-relay-integration-offer-notification"
        );
        Assert.ok(
          fxaRelayOptInPrompt,
          "Clicking on Relay auto-complete item should open the FXA + Relay opt-in prompt"
        );
        const relayTermsLink = fxaRelayOptInPrompt.querySelector(
          "#firefox-fxa-and-relay-offer-tos-url"
        );
        Assert.ok(
          relayTermsLink,
          "Relay opt-in prompt includes link to terms of service."
        );
        const relayPrivacyLink = fxaRelayOptInPrompt.querySelector(
          "#firefox-fxa-and-relay-offer-privacy-url"
        );
        Assert.ok(
          relayPrivacyLink,
          "Relay opt-in prompt includes link to privacy notice."
        );
        const relayLearnMoreLink = fxaRelayOptInPrompt.querySelector(
          ".popup-notification-learnmore-link"
        );
        Assert.ok(
          relayLearnMoreLink,
          "Relay opt-in prompt includes link to learn more."
        );
        const substrings = ["support.mozilla.org", "firefox-relay-integration"];
        Assert.ok(
          substrings.every(val => relayLearnMoreLink.href.includes(val))
        );
      }
    );
    rsSandbox.restore();
  }
);

add_task(async function test_dismiss_Relay_optin_shows_Relay_again_later() {
  const rsSandbox = await stubRemoteSettingsAllowList();
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

      const secondaryDismissButton = notificationPopup.querySelector(
        "button.popup-notification-secondary-button"
      );
      // TODO: also test the toolbarbutton.popup-notification-closebutton of the popup
      // const buttonToClick = notificationPopup.querySelector("toolbarbutton.popup-notification-closebutton");
      await clickButtonAndWaitForPopupToClose(secondaryDismissButton);

      await openACPopup(acPopup, browser, "#form-basic-username");
      const postDismissRelayItem = getRelayItemFromACPopup(acPopup);
      Assert.ok(
        postDismissRelayItem,
        "Relay item SHOULD be present in the autocomplete popup when: 1. the browser IS NOT signed in and 2. the signon.firefoxRelay.showToAllBrowsers config is set to true and 3. the user postponed the FXA + Relay opt-in popup."
      );
    }
  );
  rsSandbox.restore();
});

async function clickThruMoreActionsToDisableRelay(notificationPopup) {
  notificationPopup
    .querySelector("button.popup-notification-dropmarker")
    .click();
  const menuPopup = document.querySelector(
    "[data-l10n-id='popup-notification-more-actions-button']"
  );
  await BrowserTestUtils.waitForPopupEvent(menuPopup, "shown");
  const buttonToClick = menuPopup.querySelector("menuitem[accesskey='D']");
  await clickButtonAndWaitForPopupToClose(buttonToClick);
}

add_task(
  async function test_disable_Relay_optin_does_not_show_Relay_again_later() {
    const rsSandbox = await stubRemoteSettingsAllowList();
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
        await clickThruMoreActionsToDisableRelay(notificationPopup);

        await openACPopup(acPopup, browser, "#form-basic-username");
        const postDisableRelayItem = getRelayItemFromACPopup(acPopup);
        Assert.ok(
          !postDisableRelayItem,
          "Relay item SHOULD NOT be present in the autocomplete popup when: 1. the browser IS NOT signed in and 2. the signon.firefoxRelay.showToAllBrowsers config is set to true and 3. the user disabled the FXA + Relay opt-in popup."
        );
      }
    );
    rsSandbox.restore();

    // restore Relay to default
    await SpecialPowers.clearUserPref("signon.firefoxRelay.feature");
  }
);

add_task(
  async function test_disable_Relay_optin_can_reenable_via_preferences() {
    const rsSandbox = await stubRemoteSettingsAllowList();
    // Disable Relay from the opt-in prompt
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
        await clickThruMoreActionsToDisableRelay(notificationPopup);

        await openACPopup(acPopup, browser, "#form-basic-username");
        const postDisableRelayItem = getRelayItemFromACPopup(acPopup);
        Assert.ok(
          !postDisableRelayItem,
          "Relay item SHOULD NOT be present in the autocomplete popup when: 1. the browser IS NOT signed in and 2. the signon.firefoxRelay.showToAllBrowsers config is set to true and 3. the user disabled the FXA + Relay opt-in popup."
        );
      }
    );

    // Re-enable Relay via preferences
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
      }
    );

    // Visit the test page again and see the Relay autocomplete item is back
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
          "Relay item SHOULD be present in the autocomplete popup when 1. the browser IS NOT signed in and 2. the signon.firefoxRelay.showToAllBrowsers config is set to true and 3. the user disabled Relay but 4. the user re-enabled Relay via preferences"
        );
      }
    );
    rsSandbox.restore();
  }
);

add_task(
  async function test_unauthenticated_browser_use_email_mask_opens_fxa_signin() {
    // We need the configured signup url to set up a mock server to respond to
    // the proper path value.
    const fxaSigninUrlString =
      await gFxAccounts.constructor.config.promiseConnectAccountURI(
        "relay_integration",
        { service: "relay" }
      );
    const fxaSigninURL = new URL(fxaSigninUrlString);
    // Now that we have a URL object, we can use its components
    const fxaServer = new HttpServer();
    fxaServer.registerPathHandler(
      fxaSigninURL.URI.pathQueryRef,
      (request, response) => {
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.write("Mock FxA Sign-in page");
      }
    );
    fxaServer.start(-1);
    const mockFxaServerURL = new URL(fxaSigninUrlString);
    mockFxaServerURL.protocol = fxaServer.identity.primaryScheme + ":";
    mockFxaServerURL.hostname = fxaServer.identity.primaryHost;
    mockFxaServerURL.port = fxaServer.identity.primaryPort;
    // Override fxaccounts config to use mock server host
    await SpecialPowers.pushPrefEnv({
      set: [
        ["identity.fxaccounts.allowHttp", true],
        ["identity.fxaccounts.remote.root", mockFxaServerURL.origin],
      ],
    });

    const rsSandbox = await stubRemoteSettingsAllowList();
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
        const newTabPromise = BrowserTestUtils.waitForNewTab(
          gBrowser,
          mockFxaServerURL.href,
          true
        );

        await clickButtonAndWaitForPopupToClose(primaryButton);

        const newTab = await newTabPromise;
        const loadedUrl = newTab.linkedBrowser.currentURI.spec;
        Assert.equal(
          loadedUrl,
          mockFxaServerURL.href,
          "Clicking Use email mask should open a new tab to FXA sign-in."
        );
        BrowserTestUtils.removeTab(newTab);
      }
    );
    rsSandbox.restore();
    await new Promise(resolve => {
      fxaServer.stop(resolve);
    });
    await SpecialPowers.popPrefEnv();
  }
);
