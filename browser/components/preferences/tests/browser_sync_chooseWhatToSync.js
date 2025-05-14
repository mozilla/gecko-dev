/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Service } = ChromeUtils.importESModule(
  "resource://services-sync/service.sys.mjs"
);
const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);

// This obj will be used in both tests
// First test makes sure accepting the preferences matches these values
// Second test makes sure the cancel dialog STILL matches these values
const syncPrefs = {
  "services.sync.engine.addons": false,
  "services.sync.engine.bookmarks": true,
  "services.sync.engine.history": true,
  "services.sync.engine.tabs": false,
  "services.sync.engine.prefs": false,
  "services.sync.engine.passwords": false,
  "services.sync.engine.addresses": false,
  "services.sync.engine.creditcards": false,
};

add_setup(async () => {
  UIState._internal.notifyStateUpdated = () => {};
  const origNotifyStateUpdated = UIState._internal.notifyStateUpdated;
  const origGet = UIState.get;
  UIState.get = () => {
    return { status: UIState.STATUS_SIGNED_IN, email: "foo@bar.com" };
  };

  registerCleanupFunction(() => {
    UIState._internal.notifyStateUpdated = origNotifyStateUpdated;
    UIState.get = origGet;
  });
});

/**
 * We don't actually enable sync here, but we just check that the preferences are correct
 * when the callback gets hit (accepting/cancelling the dialog)
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1584132.
 */

add_task(async function testDialogAccept() {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  // This will check if the callback was actually called during the test
  let callbackCalled = false;

  // Enabling all the sync UI is painful in tests, so we just open the dialog manually
  let syncWindow = await openAndLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/syncChooseWhatToSync.xhtml",
    null,
    {},
    () => {
      for (const [prefKey, prefValue] of Object.entries(syncPrefs)) {
        Assert.equal(
          Services.prefs.getBoolPref(prefKey),
          prefValue,
          `${prefValue} is expected value`
        );
      }
      callbackCalled = true;
    }
  );

  Assert.ok(syncWindow, "Choose what to sync window opened");
  let syncChooseDialog =
    syncWindow.document.getElementById("syncChooseOptions");
  let syncCheckboxes = syncChooseDialog.querySelectorAll(
    "checkbox[preference]"
  );

  // Adjust the checkbox values to the expectedValues in the list
  [...syncCheckboxes].forEach(checkbox => {
    if (syncPrefs[checkbox.getAttribute("preference")] !== checkbox.checked) {
      checkbox.click();
    }
  });

  syncChooseDialog.acceptDialog();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  Assert.ok(callbackCalled, "Accept callback was called");
});

add_task(async function testDialogCancel() {
  const cancelSyncPrefs = {
    "services.sync.engine.addons": true,
    "services.sync.engine.bookmarks": false,
    "services.sync.engine.history": true,
    "services.sync.engine.tabs": true,
    "services.sync.engine.prefs": false,
    "services.sync.engine.passwords": true,
    "services.sync.engine.addresses": true,
    "services.sync.engine.creditcards": false,
  };

  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  // This will check if the callback was actually called during the test
  let callbackCalled = false;

  // Enabling all the sync UI is painful in tests, so we just open the dialog manually
  let syncWindow = await openAndLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/syncChooseWhatToSync.xhtml",
    null,
    {},
    () => {
      // We want to test against our previously accepted values in the last test
      for (const [prefKey, prefValue] of Object.entries(syncPrefs)) {
        Assert.equal(
          Services.prefs.getBoolPref(prefKey),
          prefValue,
          `${prefValue} is expected value`
        );
      }
      callbackCalled = true;
    }
  );

  ok(syncWindow, "Choose what to sync window opened");
  let syncChooseDialog =
    syncWindow.document.getElementById("syncChooseOptions");
  let syncCheckboxes = syncChooseDialog.querySelectorAll(
    "checkbox[preference]"
  );

  // This time we're adjusting to the cancel list
  [...syncCheckboxes].forEach(checkbox => {
    if (
      cancelSyncPrefs[checkbox.getAttribute("preference")] !== checkbox.checked
    ) {
      checkbox.click();
    }
  });

  syncChooseDialog.cancelDialog();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  Assert.ok(callbackCalled, "Cancel callback was called");
});

/**
 * Tests that this subdialog can be opened via
 * about:preferences?action=choose-what-to-sync#sync
 */
add_task(async function testDialogLaunchFromURI() {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", true]],
  });

  let dialogEventPromise = BrowserTestUtils.waitForEvent(
    window,
    "dialogopen",
    true
  );
  await BrowserTestUtils.withNewTab(
    "about:preferences?action=choose-what-to-sync#sync",
    async () => {
      let dialogEvent = await dialogEventPromise;
      Assert.equal(
        dialogEvent.detail.dialog._frame.contentWindow.location,
        "chrome://browser/content/preferences/dialogs/syncChooseWhatToSync.xhtml"
      );
    }
  );
});

// After CWTS is saved, we should immediately sync to update the server
add_task(async function testSyncCalledAfterSavingCWTS() {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", true]],
  });

  // Store original methods
  const svc = Weave.Service;
  const origLocked = svc._locked;
  const origSync = svc.sync;
  let syncCalls = 0;

  // Override sync functions, emulate user not currently syncing
  svc._locked = false;
  svc.sync = () => {
    syncCalls++;
    return Promise.resolve();
  };

  // Open the dialog and accept to emulate user saving their options
  await runWithCWTSDialog(async win => {
    let doc = win.document;
    let syncDialog = doc.getElementById("syncChooseOptions");

    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    syncDialog.acceptDialog();

    info("waiting for dialog to unload");
    await promiseUnloaded;

    // Since _locked is false, sync() should fire right away.
    await TestUtils.waitForCondition(
      () => syncCalls == 1,
      "Immediate sync() call when service._locked is false"
    );
  });

  // Clean up
  svc._locked = origLocked;
  svc.sync = origSync;
});

// After CWTS is saved and the user is still syncing, we should schedule a follow-up
// sync after the in-flight one
add_task(async function testSyncScheduledWhileSyncing() {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", true]],
  });

  // Store original methods
  const svc = Weave.Service;
  const origLocked = svc._locked;
  const origSync = svc.sync;
  let syncCalls = 0;

  // Override sync functions, emulate user not currently syncing
  svc._locked = true;
  svc.sync = () => {
    syncCalls++;
    return Promise.resolve();
  };

  // Open the dialog and accept to emulate user saving their options
  await runWithCWTSDialog(async win => {
    let doc = win.document;
    let syncDialog = doc.getElementById("syncChooseOptions");

    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    syncDialog.acceptDialog();

    info("waiting for dialog to unload");
    await promiseUnloaded;

    // Should *not* have called svc.sync() immediately
    Assert.equal(syncCalls, 0, "No immediate sync when _locked is true");

    // Now fire the “sync finished” notification
    Services.obs.notifyObservers(null, "weave:service:sync:finish");

    // And wait for our queued sync()
    await TestUtils.waitForCondition(
      () => syncCalls === 1,
      "Pending sync should fire once service finishes"
    );
  });

  // Clean up
  svc._locked = origLocked;
  svc.sync = origSync;
});

async function runWithCWTSDialog(test) {
  await openPreferencesViaOpenPreferencesAPI("paneSync", { leaveOpen: true });

  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/syncChooseWhatToSync.xhtml"
  );
  gBrowser.contentWindow.gSyncPane._chooseWhatToSync(true);

  let win = await promiseSubDialogLoaded;

  await test(win);

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}
