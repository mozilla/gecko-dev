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

const clickImportFromCsvMenu = async (passwordsSidebar, linesInFile) => {
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.returnValue = MockFilePicker.returnOK;
  const csvFile = await LoginTestUtils.file.setupCsvFileWithLines(linesInFile);
  await BrowserTestUtils.waitForCondition(
    () => passwordsSidebar.querySelector(".second-row"),
    "Second row failed to render"
  );
  const menu = passwordsSidebar.querySelector("panel-list");
  const menuButton = passwordsSidebar.querySelector("#more-options-menubutton");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");
  const importMenuItem = getShadowBtn(menu, "[action='import-from-file']");
  importMenuItem.click();

  async function waitForFilePicker() {
    let filePickerPromise = waitForOpenFilePicker(csvFile);
    info("Waiting for import file picker to get opened");
    await filePickerPromise;
    Assert.ok(true, "Import file picker opened");
  }

  await waitForFilePicker();
};

const waitForImportToComplete = async passwordsSidebar => {
  info("Waiting for the import to complete");
  await BrowserTestUtils.waitForCondition(
    () => passwordsSidebar.querySelector("notification-message-bar"),
    "Notification Message Bar failed to render"
  );
};

add_task(async function test_import_from_file_summary() {
  const passwordsSidebar = await openPasswordsSidebar();
  await clickImportFromCsvMenu(passwordsSidebar, VALID_CSV_LINES);
  await waitForImportToComplete(passwordsSidebar);

  const mozMessageBar = passwordsSidebar
    .querySelector("notification-message-bar")
    .shadowRoot.querySelector("moz-message-bar");
  is(mozMessageBar.type, "success", "Import succeeded");

  const summary = mozMessageBar.messageL10nArgs;
  is(summary.added, 1, "Import added one item");

  LoginTestUtils.clearData();
  info("Closing the sidebar");
  SidebarController.hide();
});

add_task(async function test_import_from_invalid_file() {
  const passwordsSidebar = await openPasswordsSidebar();
  await clickImportFromCsvMenu(passwordsSidebar, ["invalid csv"]);
  await waitForImportToComplete(passwordsSidebar);

  const mozMessageBar = passwordsSidebar
    .querySelector("notification-message-bar")
    .shadowRoot.querySelector("moz-message-bar");
  is(mozMessageBar.type, "error", "Import failed");

  info("Closing the sidebar");
  SidebarController.hide();
});
