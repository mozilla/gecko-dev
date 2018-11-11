ChromeUtils.import("resource://gre/modules/Services.jsm");

let counter = 0;

AntiTracking.runTest("Storage Access API called in a sandboxed iframe",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess();
    ok(!threw, "requestStorageAccess should not throw");
    ok(rejected, "requestStorageAccess shouldn't be available");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    // Only clear the user-interaction permissions for the tracker here so that
    // the next test has a clean slate.
    await new Promise(resolve => {
      Services.clearData.deleteDataFromHost(Services.io.newURI(TEST_3RD_PARTY_DOMAIN).host,
                                            true,
                                            Ci.nsIClearDataService.CLEAR_PERMISSIONS,
                                            value => resolve());
    });
  },
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups"
);

AntiTracking.runTest("Storage Access API called in a sandboxed iframe with" +
                     " allow-storage-access-by-user-activation",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    let [threw, rejected] = await callRequestStorageAccess();
    ok(!threw, "requestStorageAccess should not throw");
    ok(!rejected, "requestStorageAccess should be available");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    // The test harness calls this function twice.  Our cleanup function is set
    // up so that the first time that it's called, it would do the cleanup, but
    // the second time it would bail out early.  This ensures that after the
    // first time, a re-run of this test still sees the blocking notifications,
    // but also that the permission set here will be visible to the next steps
    // of the test.
    if (++counter % 2 == 0) {
      return;
    }
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, // expect blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups allow-storage-access-by-user-activation"
);

AntiTracking.runTest("Verify that sandboxed contexts don't get the saved permission",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  false, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups"
);

AntiTracking.runTest("Verify that sandboxed contexts with" +
                     " allow-storage-access-by-user-activation get the" +
                     " saved permission",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    localStorage.foo = 42;
    ok(true, "LocalStorage can be used!");
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  false, // no blocking notifications
  false, // run in normal window
  "allow-scripts allow-same-origin allow-popups allow-storage-access-by-user-activation"
);

AntiTracking.runTest("Verify that private browsing contexts don't get the saved permission",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },

  null, // non-blocking callback
  null, // cleanup function
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0, // no blocking notifications
  true, // run in private window
  null // iframe sandbox
);

AntiTracking.runTest("Verify that non-sandboxed contexts get the" +
                     " saved permission",
  // blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    localStorage.foo = 42;
    ok(true, "LocalStorage can be used!");
  },

  null, // non-blocking callback
  // cleanup function
  async _ => {
    if (++counter % 2 == 1) {
      return;
    }
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  [["dom.storage_access.enabled", true]], // extra prefs
  false, // no window open test
  false, // no user-interaction test
  0 // no blocking notifications
);
