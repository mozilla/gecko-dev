"use strict";

const { AboutWelcomeParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutWelcomeParent.sys.mjs"
);

const { AboutWelcomeTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.sys.mjs"
);
const { AWScreenUtils } = ChromeUtils.importESModule(
  "resource:///modules/aboutwelcome/AWScreenUtils.sys.mjs"
);
const { InternalTestingProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/InternalTestingProfileMigrator.sys.mjs"
);

async function clickVisibleButton(browser, selector) {
  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(browser, { selector }, async ({ selector }) => {
    function getVisibleElement() {
      for (const el of content.document.querySelectorAll(selector)) {
        if (el.offsetParent !== null) {
          return el;
        }
      }
      return null;
    }
    await ContentTaskUtils.waitForCondition(
      getVisibleElement,
      selector,
      200, // interval
      100 // maxTries
    );
    getVisibleElement().click();
  });
}

add_setup(async function () {
  SpecialPowers.pushPrefEnv({
    set: [
      ["ui.prefersReducedMotion", 1],
      ["browser.aboutwelcome.transitions", false],
    ],
  });
});

add_task(async function test_aboutwelcome_addonspicker() {
  const TEST_ADDON_CONTENT = [
    {
      id: "AW_ADDONS_PICKER",
      content: {
        position: "center",
        tiles: {
          type: "addons-picker",
          data: [
            {
              id: "addon-one-id",
              name: "uBlock Origin",
              install_label: { string_id: "btn-1-install" },
              install_complete_label: {
                string_id: "btn-1-install-complete",
              },
              icon: "",
              type: "extension",
              description: "An efficient wide-spectrum content blocker.",
              source_id: "ADD_EXTENSION_BUTTON",
              action: {
                type: "INSTALL_ADDON_FROM_URL",
                data: {
                  url: "https://test.xpi",
                  telemetrySource: "aboutwelcome-addon",
                },
              },
            },
            {
              id: "addon-two-id",
              name: "Tree-Style Tabs",
              install_label: { string_id: "btn-2-install" },
              install_complete_label: {
                string_id: "btn-2-install-complete",
              },
              icon: "",
              type: "extension",
              description: "Show tabs like a tree.",
              source_id: "ADD_EXTENSION_BUTTON",
              action: {
                type: "INSTALL_ADDON_FROM_URL",
                data: {
                  url: "https://test.xpi",
                  telemetrySource: "aboutwelcome-addon",
                },
              },
            },
            {
              id: "addon-three-id",
              name: "Pre-installed Addon",
              icon: "",
              type: "extension",
              description: "This addon has already been installed.",
              source_id: "ADD_EXTENSION_BUTTON",
              action: {
                type: "INSTALL_ADDON_FROM_URL",
                data: {
                  url: "https://test.xpi",
                  telemetrySource: "aboutwelcome-addon",
                },
              },
            },
          ],
        },
        progress_bar: true,
        logo: {},
        title: {
          raw: "Customize your Firefox",
        },
        subtitle: {
          raw: "Extensions and themes are like apps for your browser, and they let you protect passwords, download videos, find deals, block annoying ads, change how your browser looks, and much more.",
        },
        additional_button: {
          label: {
            raw: "Explore more add-ons",
          },
          style: "link",
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://test.xpi",
              where: "tab",
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2-onboarding-start-browsing-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
    {
      id: "AW_STEP_2",
      content: {
        title: "Step 4",
        primary_button: {
          label: "Next",
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: "link",
        },
      },
    },
  ];

  await setAboutWelcomeMultiStage(JSON.stringify(TEST_ADDON_CONTENT)); // NB: calls SpecialPowers.pushPrefEnv
  let { cleanup, browser } = await openMRAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  const messageSandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });
  // Stub AboutWelcomeParent's Content Message Handler
  const messageStub = messageSandbox.stub(
    aboutWelcomeActor,
    "onContentMessage"
  );

  messageStub.withArgs("AWPage:SPECIAL_ACTION").resolves(true);
  messageStub
    .withArgs("AWPage:ENSURE_ADDON_INSTALLED", "addon-one-id")
    .resolves("complete");
  messageStub
    .withArgs("AWPage:ENSURE_ADDON_INSTALLED", "addon-two-id")
    .resolves("install failed");
  messageStub
    .withArgs("AWPage:GET_INSTALLED_ADDONS")
    .resolves(["addon-three-id"]);

  // execution
  await test_screen_content(
    browser,
    "renders the addons-picker screen and tiles",
    //Expected selectors
    [
      "main.AW_ADDONS_PICKER",
      "div.addons-picker-container",
      "button[data-l10n-id='btn-1-install']",
      "button[value='secondary_button']",
      "button[value='additional_button']",
    ],

    //Unexpected selectors:
    [
      `main.screen[pos="split"]`,
      "main.AW_SET_DEFAULT",
      "button[value='primary_button']",
      "button[data-l10n-id='btn-1-install-complete']",
    ]
  );

  await clickVisibleButton(browser, ".addon-container button[value='0']"); //click the first install button

  let calls = messageStub.getCalls();
  const installExtensionCall = calls.find(
    call =>
      call.args[0] === "AWPage:SPECIAL_ACTION" &&
      call.args[1].type === "INSTALL_ADDON_FROM_URL"
  );

  info(
    `Call #${installExtensionCall}: ${
      installExtensionCall.args[0]
    } ${JSON.stringify(installExtensionCall.args[1])}`
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

  const ensureInstalledCall = calls.find(
    call =>
      call.args[0] === "AWPage:ENSURE_ADDON_INSTALLED" &&
      call.args[1] === "addon-one-id"
  );

  info(
    `--- Call #${ensureInstalledCall}: ${
      ensureInstalledCall.args[0]
    } ${JSON.stringify(ensureInstalledCall.args[1])}`
  );
  Assert.equal(
    ensureInstalledCall.args[0],
    "AWPage:ENSURE_ADDON_INSTALLED",
    "ensure addon installed was called"
  );

  await test_screen_content(
    browser,
    "renders the install complete label if addon is added successfully",
    //Expected selectors
    ["button[data-l10n-id='btn-1-install-complete']"],

    //Unexpected selectors:
    ["button[data-l10n-id='btn-1-install']"]
  );

  // Mark the first entry as having been interacted with.
  SpecialPowers.spawn(browser, [], () => {
    content.document.notifyUserGestureActivation();
  });

  // Navigate to the next screen to test state on forward/back navigation
  await clickVisibleButton(browser, "button[value='secondary_button']");
  // Update the message stub to reflect the addon install
  messageStub
    .withArgs("AWPage:GET_INSTALLED_ADDONS")
    .resolves(["addon-one-id", "addon-three-id"]);

  await TestUtils.waitForCondition(() => browser.canGoBack);
  browser.goBack();

  await test_screen_content(
    browser,
    "renders the install complete label if addon is already installed",
    //Expected selectors
    [
      "button[data-l10n-id='btn-1-install-complete']",
      "button[data-l10n-id='amo-picker-install-complete-label']",
    ],
    //Unexpected selectors:
    ["button[data-l10n-id='btn-2-install-complete']"]
  );

  await clickVisibleButton(browser, ".addon-container button[value='1']"); //click the second install button

  calls = messageStub.getCalls();
  const ensureInstalledCall2 = calls.find(
    call =>
      call.args[0] === "AWPage:ENSURE_ADDON_INSTALLED" &&
      call.args[1] === "addon-two-id"
  );

  Assert.equal(
    ensureInstalledCall2.args[0],
    "AWPage:ENSURE_ADDON_INSTALLED",
    "ensure addon installed was called"
  );

  await test_screen_content(
    browser,
    "renders the install label if addon is not added successfully",
    //Expected selectors
    ["button[data-l10n-id='btn-2-install']"],

    //Unexpected selectors:
    ["button[data-l10n-id='btn-2-install-complete']"]
  );

  // cleanup
  await SpecialPowers.popPrefEnv(); // for setAboutWelcomeMultiStage
  await cleanup();
  messageSandbox.restore();
});
