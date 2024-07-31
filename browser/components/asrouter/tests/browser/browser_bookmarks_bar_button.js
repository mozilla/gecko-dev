/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BookmarksBarButton } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/BookmarksBarButton.sys.mjs"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
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
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const browser = win.gBrowser.selectedBrowser;
  const doc = win.document;

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

  await BrowserTestUtils.closeWindow(win);
  sandbox.restore();
});

add_task(async function clickButton() {
  const message = getTestMessage();
  const Id2 = "TEST_BMB_BAR_BUTTON_2";
  message.id = Id2;
  const sandbox = sinon.createSandbox();
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const browser = win.gBrowser.selectedBrowser;
  const doc = win.document;
  const handleActionStub = sandbox.stub(SpecialMessageActions, "handleAction");

  await BookmarksBarButton.showBookmarksBarButton(browser, message);

  info("WAITING TO SHOW BOOKMARKS BAR BUTTON");

  ok(doc.querySelector(".fxms-bmb-button"), "Bookmarks Bar Button exists");

  win.document.querySelector(".fxms-bmb-button").click();

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
  await BrowserTestUtils.closeWindow(win);
  sandbox.restore();
});
