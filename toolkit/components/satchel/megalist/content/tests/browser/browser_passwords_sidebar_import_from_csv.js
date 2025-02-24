/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MockFilePicker } = SpecialPowers;

const RANDOM_NUM = Math.round(Math.random() * 100000001);

const VALID_CSV_LINES = [
  "url,username,password,httpRealm,formActionOrigin,guid,timeCreated,timeLastUsed,timePasswordChanged",
  `https://example.com,joe${RANDOM_NUM}@example.com,qwerty,My realm,,{${RANDOM_NUM}-e194-4279-ae1b-d7d281bb46f0},1589617814635,1589710449871,1589617846802`,
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contextual-password-manager.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });
  registerCleanupFunction(LoginTestUtils.clearData);
});

const waitForOpenFilePicker = destFile => {
  return new Promise(resolve => {
    MockFilePicker.showCallback = _fp => {
      info("showCallback");
      info("fileName: " + destFile.path);
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 1;
      info("done showCallback");
      resolve();
    };
  });
};

const getShadowBtn = (el, selector) =>
  el.querySelector(selector).shadowRoot.querySelector("button");

const clickImportFileMenuItem = async passwordsSidebar => {
  const menu = passwordsSidebar.querySelector("panel-list");
  const menuButton = passwordsSidebar.querySelector("#more-options-menubutton");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");
  const importMenuItem = getShadowBtn(menu, "[action='import-from-file']");
  importMenuItem.click();
};

const clickImportFromCsv = async (
  passwordsSidebar,
  linesInFile,
  isFromMenuDropdown = true
) => {
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.returnValue = MockFilePicker.returnOK;
  const csvFile = await LoginTestUtils.file.setupCsvFileWithLines(linesInFile);
  await BrowserTestUtils.waitForCondition(
    () => passwordsSidebar.querySelector(".second-row"),
    "Second row failed to render"
  );

  if (isFromMenuDropdown) {
    clickImportFileMenuItem(passwordsSidebar);
  } else {
    const importButton = passwordsSidebar.querySelector(
      ".empty-state-import-from-file"
    );
    importButton.click();
  }

  async function waitForFilePicker() {
    let filePickerPromise = waitForOpenFilePicker(csvFile);
    info("Waiting for import file picker to get opened");
    await filePickerPromise;
    Assert.ok(true, "Import file picker opened");
  }

  await waitForFilePicker();
};

add_task(async function test_import_from_file_summary() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const passwordsSidebar = await openPasswordsSidebar();
  await clickImportFromCsv(passwordsSidebar, VALID_CSV_LINES);

  let events = Glean.contextualManager.toolbarAction.testGetValue();
  assertCPMGleanEvent(events[0], {
    trigger: "toolbar",
    option_name: "import_file",
  });

  const notifMsgBar = await checkNotificationAndTelemetry(
    passwordsSidebar,
    "import-success"
  );
  const mozMessageBar = notifMsgBar.shadowRoot.querySelector("moz-message-bar");
  const summary = mozMessageBar.messageL10nArgs;
  is(summary.added, 1, "Import added one item");
  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "import_success",
    action_type: "dismiss",
  });

  LoginTestUtils.clearData();
  info("Closing the sidebar");
  SidebarController.hide();
});

add_task(async function test_import_from_invalid_file() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const passwordsSidebar = await openPasswordsSidebar();
  await clickImportFromCsv(passwordsSidebar, ["invalid csv"]);
  const notifMsgBar = await checkNotificationAndTelemetry(
    passwordsSidebar,
    "import-error"
  );
  checkNotificationInteractionTelemetry(notifMsgBar, "primary-action", {
    notification_detail: "import_error",
    action_type: "import",
  });
  checkNotificationInteractionTelemetry(
    notifMsgBar,
    "secondary-action",
    {
      notification_detail: "import_error",
      action_type: "dismiss",
    },
    1
  );

  LoginTestUtils.clearData();
  info("Closing the sidebar");
  SidebarController.hide();
});

add_task(async function test_import_empty_state() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const passwordsSidebar = await openPasswordsSidebar();
  await checkEmptyState(".no-logins-card-content", passwordsSidebar);
  await clickImportFromCsv(passwordsSidebar, VALID_CSV_LINES, false);

  let events = Glean.contextualManager.toolbarAction.testGetValue();
  assertCPMGleanEvent(events[0], {
    trigger: "empty_state_card",
    option_name: "import_file",
  });

  await checkNotificationAndTelemetry(passwordsSidebar, "import-success");

  const mozMessageBar = passwordsSidebar
    .querySelector("notification-message-bar")
    .shadowRoot.querySelector("moz-message-bar");
  is(mozMessageBar.type, "success", "Import succeeded");

  const summary = mozMessageBar.messageL10nArgs;
  is(summary.added, 1, "Import added one item");

  let updateEvents = Glean.contextualManager.recordsUpdate.testGetValue();
  Assert.equal(updateEvents.length, 1, "Recorded import passwords once.");
  assertCPMGleanEvent(updateEvents[0], {
    change_type: "import",
  });

  info("Closing the sidebar");
  SidebarController.hide();
});
