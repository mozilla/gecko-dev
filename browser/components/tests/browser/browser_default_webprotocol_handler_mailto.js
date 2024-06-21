/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const WebProtocolHandlerRegistrar = ChromeUtils.importESModule(
  "resource:///modules/WebProtocolHandlerRegistrar.sys.mjs"
).WebProtocolHandlerRegistrar.prototype;

add_setup(async () => {
  Services.prefs.setCharPref("browser.protocolhandler.loglevel", "debug");
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.protocolhandler.loglevel");
    // remove all permissions in the permission manager, where we stored
    // a timeout after clicking the 'x' and 'not now' buttons.
    Services.perms.removeAll();
  });

  Services.prefs.clearUserPref("browser.mailto.dualPrompt");
  Services.prefs.clearUserPref("browser.mailto.dualPrompt.onLocationChange");

  await ExperimentAPI.ready();

  WebProtocolHandlerRegistrar._addWebProtocolHandler(
    protocol,
    "example.com",
    "example.com"
  );
});

const protocol = "mailto";
const test_domain = "example.com";

const selector_mailto_prompt =
  'notification-message[message-bar-type="infobar"]' +
  '[value="OS Protocol Registration: mailto"]';

add_task(async function check_installHash() {
  Assert.notEqual(
    null,
    WebProtocolHandlerRegistrar._getInstallHash(),
    "test to check the installHash"
  );
});

add_task(async function check_addWebProtocolHandler() {
  let currentHandler = WebProtocolHandlerRegistrar._addWebProtocolHandler(
    protocol,
    test_domain,
    "https://" + test_domain
  );

  Assert.equal(
    test_domain,
    currentHandler.name,
    "does the handler have the right name?"
  );
  Assert.equal(
    "https://" + test_domain,
    currentHandler.uriTemplate,
    "does the handler have the right uri?"
  );

  WebProtocolHandlerRegistrar._setProtocolHandlerDefault(
    protocol,
    currentHandler
  );
  WebProtocolHandlerRegistrar.removeProtocolHandler(
    protocol,
    currentHandler.uriTemplate
  );
});

/* The next test is to ensure that we offer users to configure a webmailer
 * instead of a custom executable. They will not get a prompt if they have
 * configured their OS as default handler.
 */
add_task(async function promptShownForLocalHandler() {
  let handlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  handlerApp.executable = Services.dirsvc.get("XREExeF", Ci.nsIFile);
  WebProtocolHandlerRegistrar._setProtocolHandlerDefault(protocol, handlerApp);

  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    await WebProtocolHandlerRegistrar._askUserToSetMailtoHandler(
      browser,
      protocol,
      Services.io.newURI("https://example.com"),
      "https://example.com"
    );

    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "The prompt is shown when an executable is configured as handler."
    );
  });
});

function test_rollout(
  dualPrompt = false,
  onLocationChange = false,
  dismissNotNowMinutes = 15,
  dismissXClickMinutes = 15
) {
  return ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.mailto.featureId,
      value: {
        dualPrompt,
        "dualPrompt.onLocationChange": onLocationChange,
        "dualPrompt.dismissXClickMinutes": dismissXClickMinutes,
        "dualPrompt.dismissNotNowMinutes": dismissNotNowMinutes,
      },
    },
    { isRollout: true }
  );
}

add_task(async function check_no_button() {
  let cleanup = await test_rollout(true, true);

  const url = "https://" + test_domain;
  WebProtocolHandlerRegistrar._addWebProtocolHandler(
    protocol,
    test_domain,
    url
  );

  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "The prompt is shown with dualPrompt.onLocationChange toggled on."
    );
  });

  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    let button_no = document.querySelector(
      "[data-l10n-id='protocolhandler-mailto-os-handler-no-button']"
    );
    Assert.notEqual(null, button_no, "is the no-button there?");

    await button_no.click();
    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt hidden after button_no clicked."
    );

    await WebProtocolHandlerRegistrar._askUserToSetMailtoHandler(
      browser,
      protocol,
      Services.io.newURI("https://example.com"),
      "https://example.com"
    );

    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt stays hidden even when called after the no button was clicked."
    );
  });

  cleanup();
});

add_task(async function check_x_button() {
  let timeout_x = 15;
  let cleanup = await test_rollout(true, true, 0, timeout_x);
  Services.perms.removeAll();

  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt gets shown again- the timeout for the no_button was set to zero"
    );

    let button_x = document
      .querySelector(".infobar")
      .shadowRoot.querySelector(".close");
    Assert.notEqual(null, button_x, "is the x-button there?");

    await button_x.click();
    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt hidden after 'X' button clicked."
    );

    await WebProtocolHandlerRegistrar._askUserToSetMailtoHandler(
      browser,
      protocol,
      Services.io.newURI("https://example.com"),
      "https://example.com"
    );

    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt stays hidden even when called after the no button was clicked."
    );

    // we expect that this test does not take more than a minute
    let expireTime = Services.perms.getPermissionObject(
      browser.contentPrincipal,
      "mailto-infobar-dismissed",
      true
    ).expireTime;

    Assert.equal(
      timeout_x * 60,
      ((expireTime - Date.now()) / 1000).toFixed(),
      "test completed within one minute, confirmed by the time after" +
        " which the permission manager would show the bar again after a dismiss."
    );
  });

  cleanup();
});

add_task(async function check_x_button() {
  let cleanup = await test_rollout(true, true, 0, 0);
  Services.perms.removeByType("mailto-infobar-dismissed");

  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "infobar shown after reset timeout for 'mailto-infobar-dismissed'" +
        " in permission manager."
    );
  });

  cleanup();
});

add_task(async function check_bar_is_not_shown() {
  let cleanup = await test_rollout(true, false);

  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "Prompt is not shown, because the dualPrompt.onLocationChange is off"
    );
  });

  cleanup();
});
