"use strict";

const { ASRouter } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouter.sys.mjs"
);
const { AttributionCode } = ChromeUtils.importESModule(
  "resource:///modules/AttributionCode.sys.mjs"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

const TEST_ATTRIBUTION_DATA = {
  campaign: "set_default_browser",
};

const DID_HANDLE_CAMAPAIGN_ACTION_PREF =
  "trailhead.firstrun.didHandleCampaignAction";

const TEST_PROTON_CONTENT = [
  {
    id: "AW_STEP1",
    content: {
      title: "Step 1",
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
    },
  },
];

add_task(async function test_unhandled_campaign_action() {
  const sandbox = sinon.createSandbox();
  const handleActionStub = sandbox
    .stub(SpecialMessageActions, "handleAction")
    .resolves();

  await AttributionCode.deleteFileAsync();
  await ASRouter.forceAttribution(TEST_ATTRIBUTION_DATA);
  const TEST_PROTON_JSON = JSON.stringify(TEST_PROTON_CONTENT);

  await setAboutWelcomePref(true);
  await pushPrefs(["browser.aboutwelcome.screens", TEST_PROTON_JSON]);

  AttributionCode._clearCache();
  const data = await AttributionCode.getAttrDataAsync();

  Assert.equal(
    data.campaign,
    "set_default_browser",
    "Attribution campaign should be set"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  await TestUtils.waitForCondition(() => handleActionStub.called);

  Assert.equal(
    handleActionStub.firstCall.args[0].type,
    "SET_DEFAULT_BROWSER",
    "Set default special message action is called"
  );

  Assert.equal(
    Services.prefs.getBoolPref(DID_HANDLE_CAMAPAIGN_ACTION_PREF, false),
    true,
    "Set default campaign action handled pref is set to true"
  );

  handleActionStub.reset();
  // Open a new about:welcome tab to ensure the action does not run again
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  sinon.assert.notCalled(handleActionStub);

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    BrowserTestUtils.removeTab(tab2);
    await ASRouter.forceAttribution("");
    Services.prefs.clearUserPref(DID_HANDLE_CAMAPAIGN_ACTION_PREF);
    Services.prefs.clearUserPref("browser.aboutwelcome.screens");

    sandbox.restore();
  });
});
