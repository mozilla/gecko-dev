/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ChromeProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/ChromeProfileMigrator.sys.mjs"
);
const { LoginCSVImport } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginCSVImport.sys.mjs"
);

const TEST_FILE_PATH = getTestFilePath("dummy_file.csv");

// We use MockFilePicker to simulate a native file picker, and prepare it
// to return a dummy file pointed at TEST_FILE_PATH. The file at
// TEST_FILE_PATH is not required (nor expected) to exist.
const { MockFilePicker } = SpecialPowers;

add_setup(async function () {
  MockFilePicker.init(window.browsingContext);
  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

/**
 * A helper function that does most of the heavy lifting for the tests in
 * this file. Specfically, it takes care of:
 *
 * 1. Stubbing out the various hunks of the ChromeProfileMigrator in order
 *    to simulate a migration without actually performing one, since the
 *    migrator itself isn't being tested here.
 * 2. Stubbing out parts of MigrationUtils and LoginCSVImport to have a
 *    consistent reporting on how many things are imported.
 * 3. Setting up the MockFilePicker if expectsFilePicker is true to return
 *    the TEST_FILE_PATH.
 * 4. Opens up the migration wizard, and chooses to import both BOOKMARKS
 *    and PASSWORDS, and then clicks "Import".
 * 5. Waits for the migration wizard to show the Chrome password import
 *    instructions.
 * 6. Runs taskFn
 * 7. Closes the migration dialog.
 *
 * @param {boolean} expectsFilePicker
 *   True if the MockFilePicker should be set up to return TEST_FILE_PATH.
 * @param {boolean} migrateBookmarks
 *   True if bookmarks should be migrated alongside passwords. If not, only
 *   passwords will be migrated.
 * @param {boolean} shouldPasswordImportFail
 *   True if importing from the CSV file should fail.
 * @param {Function} taskFn
 *   An asynchronous function that takes the following parameters in this
 *   order:
 *
 *    {Element} wizard
 *      The opened migration wizard
 *    {Promise} filePickerShownPromise
 *      A Promise that resolves once the MockFilePicker has closed. This is
 *      undefined if expectsFilePicker was false.
 *    {object} importFromCSVStub
 *      The Sinon stub object for LoginCSVImport.importFromCSV. This can be
 *      used to check to see whether it was called.
 *    {Promise} didMigration
 *      A Promise that resolves to true once the migration completes.
 *    {Promise} wizardDone
 *      A Promise that resolves once the migration wizard reports that a
 *      migration has completed.
 * @returns {Promise<undefined>}
 */
async function testChromePasswordHelper(
  expectsFilePicker,
  migrateBookmarks,
  shouldPasswordImportFail,
  taskFn
) {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  sandbox.stub(MigrationUtils, "availableMigratorKeys").get(() => {
    return ["chrome"];
  });

  let migrator = new ChromeProfileMigrator();
  sandbox.stub(MigrationUtils, "getMigrator").resolves(migrator);

  sandbox
    .stub(ChromeProfileMigrator.prototype, "getSourceProfiles")
    .resolves([{ id: "chrome-test-1", name: "Chrome test profile 1" }]);

  // Have the migrator claim that only BOOKMARKS are available.
  sandbox
    .stub(ChromeProfileMigrator.prototype, "getMigrateData")
    .resolves(MigrationUtils.resourceTypes.BOOKMARKS);

  let migrateStub;
  let didMigration = new Promise(resolve => {
    migrateStub = sandbox
      .stub(ChromeProfileMigrator.prototype, "migrate")
      .callsFake((aResourceTypes, aStartup, aProfile, aProgressCallback) => {
        if (!migrateBookmarks) {
          Assert.ok(
            false,
            "Should not have called migrate when only migrating Chrome passwords."
          );
        }

        Assert.ok(
          !aStartup,
          "Migrator should not have been called as a startup migration."
        );
        Assert.ok(
          aResourceTypes & MigrationUtils.resourceTypes.BOOKMARKS,
          "Should have requested to migrate the BOOKMARKS resource."
        );
        Assert.ok(
          !(aResourceTypes & MigrationUtils.resourceTypes.PASSWORDS),
          "Should not have requested to migrate the PASSWORDS resource."
        );

        aProgressCallback(MigrationUtils.resourceTypes.BOOKMARKS, true);
        Services.obs.notifyObservers(null, "Migration:Ended");
        resolve();
      });
  });

  // We'll pretend we added EXPECTED_QUANTITY passwords from the Chrome
  // password file.
  let results = [];
  for (let i = 0; i < EXPECTED_QUANTITY; ++i) {
    results.push({ result: "added" });
  }
  let importFromCSVStub = sandbox.stub(LoginCSVImport, "importFromCSV");

  if (shouldPasswordImportFail) {
    importFromCSVStub.rejects(new Error("Some error message"));
  } else {
    importFromCSVStub.resolves(results);
  }

  sandbox.stub(MigrationUtils, "_importQuantities").value({
    bookmarks: EXPECTED_QUANTITY,
  });

  let filePickerShownPromise;

  if (expectsFilePicker) {
    MockFilePicker.reset();

    let dummyFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dummyFile.initWithPath(TEST_FILE_PATH);
    filePickerShownPromise = new Promise(resolve => {
      MockFilePicker.showCallback = () => {
        Assert.ok(true, "Filepicker shown.");
        MockFilePicker.setFiles([dummyFile]);
        resolve();
      };
    });
    MockFilePicker.returnValue = MockFilePicker.returnOK;
  }

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );

    let shadow = wizard.openOrClosedShadowRoot;

    info("Choosing Chrome");
    let panelItem = shadow.querySelector(
      `panel-item[key="${ChromeProfileMigrator.key}"]`
    );
    panelItem.click();

    let resourceTypeList = shadow.querySelector("#resource-type-list");

    // Let's choose whether to import BOOKMARKS first.
    let bookmarksNode = resourceTypeList.querySelector(
      `label[data-resource-type="${MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS}"]`
    );
    bookmarksNode.control.checked = migrateBookmarks;

    // Let's make sure that PASSWORDS is displayed despite the migrator only
    // (currently) returning BOOKMARKS as an available resource to migrate.
    let passwordsNode = resourceTypeList.querySelector(
      `label[data-resource-type="${MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS}"]`
    );
    Assert.ok(
      !passwordsNode.hidden,
      "PASSWORDS should be available to import from."
    );
    passwordsNode.control.checked = true;

    let deck = shadow.querySelector("#wizard-deck");
    let switchedToChromePermissionPage =
      BrowserTestUtils.waitForMutationCondition(
        deck,
        { attributeFilter: ["selected-view"] },
        () => {
          return (
            deck.getAttribute("selected-view") ==
            "page-" +
              MigrationWizardConstants.PAGES.CHROME_WINDOWS_PASSWORD_PERMISSION
          );
        }
      );

    let importButton = shadow.querySelector("#import");
    importButton.click();
    await switchedToChromePermissionPage;
    Assert.ok(true, "Went to Chrome permission page after attempting import.");

    await taskFn(
      wizard,
      filePickerShownPromise,
      importFromCSVStub,
      didMigration,
      migrateStub,
      wizardDone
    );

    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let doneButton = shadow.querySelector(
      "div[name='page-progress'] .done-button"
    );
    let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");

    doneButton.click();
    await dialogClosed;
  });

  sandbox.restore();
  MockFilePicker.reset();
}

/**
 * Tests the flow of importing passwords from Chrome via an
 * exported CSV file.
 */
add_task(async function test_chrome_password_do_import() {
  await testChromePasswordHelper(
    true,
    true,
    false,
    async (
      wizard,
      filePickerShownPromise,
      importFromCSVStub,
      didMigration,
      migrateStub,
      wizardDone
    ) => {
      let shadow = wizard.openOrClosedShadowRoot;
      let manualPasswordImportSelect = shadow.querySelector(
        "div[name='page-chrome-windows-password-permission'] .manual-password-import-select"
      );

      manualPasswordImportSelect.click();
      await filePickerShownPromise;
      Assert.ok(true, "File picker was shown.");

      await didMigration;
      Assert.ok(importFromCSVStub.called, "Importing from CSV was called.");

      await wizardDone;

      assertQuantitiesShown(wizard, [
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
      ]);
    }
  );
});

/**
 * Tests that only passwords get imported if the user only opts
 * to import passwords, and that nothing else gets imported.
 */
add_task(async function test_chrome_password_only_do_import() {
  await testChromePasswordHelper(
    true,
    false,
    false,
    async (
      wizard,
      filePickerShownPromise,
      importFromCSVStub,
      didMigration,
      migrateStub,
      wizardDone
    ) => {
      let shadow = wizard.openOrClosedShadowRoot;
      let manualPasswordImportSelect = shadow.querySelector(
        "div[name='page-chrome-windows-password-permission'] .manual-password-import-select"
      );
      manualPasswordImportSelect.click();
      await filePickerShownPromise;
      Assert.ok(true, "File picker was shown.");

      await wizardDone;

      assertQuantitiesShown(wizard, [
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
      ]);

      Assert.ok(importFromCSVStub.called, "Importing from CSV was called.");
      Assert.ok(
        !migrateStub.called,
        "ChromeProfileMigrator.migrate was not called."
      );
    }
  );
});

/**
 * Tests the flow of importing passwords from Chrome when the file
 * import fails.
 */
add_task(async function test_chrome_password_empty_csv_file() {
  await testChromePasswordHelper(
    true,
    true,
    true,
    async (
      wizard,
      filePickerShownPromise,
      importFromCSVStub,
      didMigration,
      migrateStub,
      wizardDone
    ) => {
      let shadow = wizard.openOrClosedShadowRoot;
      let manualPasswordImportSelect = shadow.querySelector(
        "div[name='page-chrome-windows-password-permission'] .manual-password-import-select"
      );
      manualPasswordImportSelect.click();
      await filePickerShownPromise;
      Assert.ok(true, "File picker was shown.");

      await didMigration;
      Assert.ok(importFromCSVStub.called, "Importing from CSV was called.");

      await wizardDone;

      let headerL10nID =
        shadow.querySelector("#progress-header").dataset.l10nId;
      Assert.equal(
        headerL10nID,
        "migration-wizard-progress-done-with-warnings-header"
      );

      let progressGroup = shadow.querySelector(
        `.resource-progress-group[data-resource-type="${MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS}"`
      );
      let progressIcon = progressGroup.querySelector(".progress-icon");
      let messageText =
        progressGroup.querySelector(".message-text").textContent;

      Assert.equal(
        progressIcon.getAttribute("state"),
        "warning",
        "Icon should be in the warning state."
      );
      Assert.stringMatches(
        messageText,
        /file doesn’t include any valid password data/
      );
    }
  );
});

/**
 * Tests that the user can skip importing passwords from Chrome.
 */
add_task(async function test_chrome_password_skip() {
  await testChromePasswordHelper(
    false,
    true,
    false,
    async (
      wizard,
      filePickerShownPromise,
      importFromCSVStub,
      didMigration,
      migrateStub,
      wizardDone
    ) => {
      let shadow = wizard.openOrClosedShadowRoot;
      let manualPasswordImportSkip = shadow.querySelector(
        "div[name='page-chrome-windows-password-permission'] .manual-password-import-skip"
      );
      manualPasswordImportSkip.click();

      await didMigration;
      Assert.ok(!MockFilePicker.shown, "Never showed the file picker.");
      Assert.ok(
        !importFromCSVStub.called,
        "Importing from CSV was never called."
      );

      await wizardDone;

      assertQuantitiesShown(wizard, [
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
      ]);
    }
  );
});
