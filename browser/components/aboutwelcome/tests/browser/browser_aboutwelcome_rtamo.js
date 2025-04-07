"use strict";

const { ASRouter } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouter.sys.mjs"
);
const { AddonRepository } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/AddonRepository.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const TEST_ADDON_INFO = [
  {
    name: "Test Add-on",
    sourceURI: { scheme: "https", spec: "https://test.xpi" },
    icons: { 32: "test.png", 64: "test.png" },
    type: "extension",
  },
];

const TEST_ADDON_INFO_THEME = [
  {
    name: "Test Add-on",
    sourceURI: { scheme: "https", spec: "https://test.xpi" },
    icons: { 32: "test.png", 64: "test.png" },
    screenshots: [{ url: "test.png" }],
    type: "theme",
  },
];

async function openRTAMOWelcomePage() {
  // Can't properly stub the child/parent actors so instead
  // we stub the modules they depend on for the RTAMO flow
  // to ensure the right thing is rendered.
  await ASRouter.forceAttribution({
    source: "addons.mozilla.org",
    medium: "referral",
    campaign: "non-fx-button",
    // with the sinon override, the id doesn't matter
    content: "rta:whatever",
    experiment: "ua-onboarding",
    variation: "chrome",
    ua: "Google Chrome 123",
    dltoken: "00000000-0000-0000-0000-000000000000",
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    // Clear cache call is only possible in a testing environment
    Services.env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");
    await ASRouter.forceAttribution({
      source: "",
      medium: "",
      campaign: "",
      content: "",
      experiment: "",
      variation: "",
      ua: "",
      dltoken: "",
    });
  });

  return tab.linkedBrowser;
}

/**
 * Setup and test RTAMO welcome UI
 */
async function test_screen_content(
  browser,
  experiment,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
  await ContentTask.spawn(
    browser,
    { expectedSelectors, experiment, unexpectedSelectors },
    async ({
      expectedSelectors: expected,
      experiment: experimentName,
      unexpectedSelectors: unexpected,
    }) => {
      for (let selector of expected) {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(selector),
          `Should render ${selector} in ${experimentName}`
        );
      }
      for (let selector of unexpected) {
        ok(
          !content.document.querySelector(selector),
          `Should not render ${selector} in ${experimentName}`
        );
      }
    }
  );
}

/**
 * Test the RTAMO welcome UI
 */
add_task(async function test_rtamo_aboutwelcome() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(AddonRepository, "getAddonsByIDs").resolves(TEST_ADDON_INFO);

  let browser = await openRTAMOWelcomePage();

  await test_screen_content(
    browser,
    "RTAMO UI",
    // Expected selectors:
    [
      `div.onboardingContainer[style*='background: var(--mr-welcome-background-color) var(--mr-welcome-background-gradient)']`,
      "h2[data-l10n-id='mr1-return-to-amo-addon-title']",
      `h2[data-l10n-args='{"addon-name":"${TEST_ADDON_INFO[0].name}"}'`,
      "div.rtamo-icon",
      "button.primary[data-l10n-id='mr1-return-to-amo-add-extension-label']",
      "button[data-l10n-id='mr2022-onboarding-secondary-skip-button-label']",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP1",
      "main.AW_STEP2",
      "main.AW_STEP3",
      "div.tiles-container.info",
    ]
  );

  let windowGlobalParent = browser.browsingContext.currentWindowGlobal;
  let aboutWelcomeActor = windowGlobalParent.getActor("AboutWelcome");
  const messageSandbox = sinon.createSandbox();
  // Stub AboutWelcomeParent Content Message Handler
  messageSandbox.stub(aboutWelcomeActor, "onContentMessage");
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });

  await onButtonClick(browser, "button.primary");
  const { callCount } = aboutWelcomeActor.onContentMessage;

  Assert.strictEqual(
    callCount,
    4,
    `${callCount} Stub called four times to send telemetry, set url, install extension and verify install`
  );

  const sendTelemetryCall = aboutWelcomeActor.onContentMessage.getCall(0);
  const setUrlCall = aboutWelcomeActor.onContentMessage.getCall(1);
  const installExtensionCall = aboutWelcomeActor.onContentMessage.getCall(2);

  Assert.equal(
    sendTelemetryCall.args[0],
    "AWPage:TELEMETRY_EVENT",
    "send event telemetry"
  );
  Assert.equal(
    sendTelemetryCall.args[1].event,
    "CLICK_BUTTON",
    "Telemetry event sent as INSTALL"
  );
  Assert.equal(
    setUrlCall.args[0],
    "AWPage:SPECIAL_ACTION",
    "send special action to set extension URL"
  );
  Assert.equal(
    installExtensionCall.args[0],
    "AWPage:SPECIAL_ACTION",
    "send special action to install add on"
  );
  Assert.equal(
    installExtensionCall.args[1].type,
    "INSTALL_ADDON_FROM_URL",
    "Special action type is INSTALL_ADDON_FROM_URL"
  );
  Assert.equal(
    installExtensionCall.args[1].data.url,
    "https://test.xpi",
    "Install add on url"
  );

  sandbox.restore();
});

add_task(async function test_rtamo_primary_button_theme() {
  let themeSandbox = sinon.createSandbox();
  themeSandbox
    .stub(AddonRepository, "getAddonsByIDs")
    .resolves(TEST_ADDON_INFO_THEME);

  let browser = await openRTAMOWelcomePage();

  await test_screen_content(
    browser,
    "RTAMO UI",
    // Expected selectors:
    [
      "div.onboardingContainer",
      "h2[data-l10n-id='mr1-return-to-amo-addon-title']",
      "div.rtamo-icon",
      "button.primary[data-l10n-id='return-to-amo-add-theme-label']",
      "button[data-l10n-id='mr2022-onboarding-secondary-skip-button-label']",
      "img.rtamo-theme-icon",
    ],
    // Unexpected selectors:
    [
      "main.AW_STEP1",
      "main.AW_STEP2",
      "main.AW_STEP3",
      "div.tiles-container.info",
    ]
  );

  themeSandbox.restore();
});
