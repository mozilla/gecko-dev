/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BookmarksBarButton } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/BookmarksBarButton.sys.mjs"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const surfaceName = "fxms-bmb-button";

function getTestMessage() {
  return {
    id: "TEST_BMB_BAR_BUTTON",
    groups: [],
    template: "bookmarks_bar_button",
    content: {
      label: {
        raw: "Getting Started",
        tooltiptext: "Getting started with Firefox",
      },
      action: {
        type: "OPEN_URL",
        data: {
          args: "https://www.mozilla.org",
          where: "tab",
        },
        navigate: true,
      },
    },
    trigger: { id: "defaultBrowserCheck" },
    frequency: {
      lifetime: 100,
    },
    targeting: "true",
  };
}

async function assertTelemetryScalars(expectedScalars) {
  let processScalars =
    Services.telemetry.getSnapshotForKeyedScalars("main", true)?.parent ?? {};
  let expectedKeys = Object.keys(expectedScalars);

  //key is something like "browser.ui.customized_widgets"
  for (const key of expectedKeys) {
    const expectedEvents = expectedScalars[key];
    const actualEvents = processScalars[key];

    for (const eventKey of Object.keys(expectedEvents)) {
      Assert.equal(
        expectedEvents[eventKey],
        actualEvents[eventKey],
        `Expected to see the correct value for scalar ${eventKey}, got ${actualEvents[eventKey]}`
      );
    }
  }
}

add_task(async function showButton() {
  const message = getTestMessage();
  const ID = message.id;
  const sandbox = sinon.createSandbox();
  const browser = gBrowser.selectedBrowser;
  const doc = document;

  await BookmarksBarButton.showBookmarksBarButton(browser, message);

  info("WAITING TO SHOW BOOKMARKS BAR BUTTON");

  ok(doc.querySelector(".fxms-bmb-button"), "Bookmarks Bar Button exists");

  // Check Keyed Scalars for Telemetry
  const expectedScalars = {
    "TEST-BMB-BAR-BUTTON_add_na_bookmarks-bar_create": 1,
    "TEST-BMB-BAR-BUTTON_remove_bookmarks-bar_na_destroy": 1,
  };

  assertTelemetryScalars(expectedScalars);

  CustomizableUI.destroyWidget(ID);
  await CustomizableUI.reset();

  sandbox.restore();
});

add_task(async function clickButton() {
  const message = getTestMessage();
  const Id2 = "TEST_BMB_BAR_BUTTON_2";
  message.id = Id2;
  const sandbox = sinon.createSandbox();
  const browser = gBrowser.selectedBrowser;
  const doc = document;
  const handleActionStub = sandbox.stub(SpecialMessageActions, "handleAction");

  await BookmarksBarButton.showBookmarksBarButton(browser, message);

  info("WAITING TO SHOW BOOKMARKS BAR BUTTON");

  ok(doc.querySelector(".fxms-bmb-button"), "Bookmarks Bar Button exists");

  doc.querySelector(".fxms-bmb-button").click();

  ok(
    handleActionStub.calledWith(message.content.action),
    "handleAction should be called with correct Args"
  );
  ok(!doc.querySelector(".fxms-bmb-button"), "button should be removed");

  // Check Keyed Scalars for Telemetry
  const expectedClickScalars = {
    "browser.ui.customized_widgets": {
      "TEST-BMB-BAR-BUTTON-2_remove_bookmarks-bar_na_destroy": 1,
      "TEST-BMB-BAR-BUTTON-2_add_na_bookmarks-bar_create": 1,
    },
    "browser.ui.interaction.bookmarks_bar": {
      "TEST-BMB-BAR-BUTTON-2": 1,
    },
  };

  assertTelemetryScalars(expectedClickScalars);

  CustomizableUI.destroyWidget(Id2);
  await CustomizableUI.reset();
  sandbox.restore();
});

add_task(async function supportedActionsOnly() {
  const message = getTestMessage();
  const Id3 = "TEST_BMB_BAR_BUTTON_3";
  message.id = Id3;
  message.content.action = {
    type: "MULTI_ACTION",
    navigate: true,
    data: {
      actions: [
        {
          type: "SET_PREF",
          data: {
            pref: {
              name: "testpref.test.test",
              value: true,
            },
          },
        },
        {
          type: "DUMMY_ACTION",
          data: {
            args: "https://www.mozilla.org",
            where: "tab",
          },
        },
      ],
    },
  };

  const sandbox = sinon.createSandbox();
  const browser = gBrowser.selectedBrowser;
  const doc = document;
  const handleActionStub = sandbox.stub(SpecialMessageActions, "handleAction");

  await BookmarksBarButton.showBookmarksBarButton(browser, message);

  info("WAITING TO SHOW BOOKMARKS BAR BUTTON");

  ok(doc.querySelector(".fxms-bmb-button"), "Bookmarks Bar Button exists");

  doc.querySelector(".fxms-bmb-button").click();

  ok(
    handleActionStub.notCalled,
    "handleAction should not be called if invalid action type was provided"
  );

  CustomizableUI.destroyWidget(Id3);
  await CustomizableUI.reset();
  sandbox.restore();
});

add_task(async function customizableIconLogo() {
  const message = getTestMessage();
  const Id4 = "TEST_BMB_BAR_BUTTON_4";
  message.id = Id4;
  let iconUrl = "chrome://browser/content/callout-tab-pickup.svg";
  message.content.logo = {
    imageURL: iconUrl,
  };

  const sandbox = sinon.createSandbox();
  const browser = gBrowser.selectedBrowser;
  const doc = document;

  await BookmarksBarButton.showBookmarksBarButton(browser, message);

  info("WAITING TO SHOW BOOKMARKS BAR BUTTON");

  ok(doc.querySelector(".fxms-bmb-button"), "Bookmarks Bar Button exists");
  Assert.strictEqual(
    doc.querySelector(".fxms-bmb-button").style.listStyleImage,
    'url("chrome://browser/content/callout-tab-pickup.svg")',
    `Should use customizable logo URL`
  );

  CustomizableUI.destroyWidget(Id4);
  await CustomizableUI.reset();
  sandbox.restore();
});

add_task(async function test_bookmarks_bar_button_experiment_update() {
  const featureId = "fxms_bmb_button";
  const sandbox = sinon.createSandbox();
  const browser = gBrowser.selectedBrowser;
  const doc = document;
  const buttonClass = ".fxms-bmb-button";

  const message = getTestMessage();

  // enroll in an experiment with the bookmarks bar button feature
  const cleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId,
    value: message,
  });

  // display the bookmarks bar button, and verify its presence
  await BookmarksBarButton.showBookmarksBarButton(browser, message);
  ok(doc.querySelector(buttonClass), "Bookmarks Bar Button exists");

  // unenroll from the experiment
  cleanup();

  // wait for the button to be removed, and verify that it's gone
  await TestUtils.waitForCondition(
    () => !doc.querySelector(buttonClass),
    "Bookmarks Bar Button is removed after experiment is unenrolled"
  );

  await CustomizableUI.reset();
  sandbox.restore();
});
