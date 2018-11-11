ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Set/Get Cookies",
  // Blocking callback
  async _ => {
    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "No cookies for me");

    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });
    // Let's do it twice.
    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });

    is(document.cookie, "", "Still no cookies for me");
  },

  // Non blocking callback
  async _ => {
    is(document.cookie, "", "No cookies for me");

    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });

    document.cookie = "name=value";
    ok(document.cookie.includes("name=value"), "Some cookies for me");
    ok(document.cookie.includes("foopy=1"), "Some cookies for me");

    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-present", "We should have cookies");
    });

    ok(document.cookie.length, "Some Cookies for me");
  },

  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

AntiTracking.runTest("Cookies and Storage Access API",
  // Blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "No cookies for me");

    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });
    // Let's do it twice.
    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });

    is(document.cookie, "", "Still no cookies for me");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "name=value", "I have the cookies!");
  },

  // Non blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");

    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });

    document.cookie = "name=value";
    ok(document.cookie.includes("name=value"), "Some cookies for me");
    ok(document.cookie.includes("foopy=1"), "Some cookies for me");

    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-present", "We should have cookies");
    });

    ok(document.cookie.length, "Some Cookies for me");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    ok(document.cookie.length, "Still some Cookies for me");
    ok(document.cookie.includes("name=value"), "Some cookies for me");
    ok(document.cookie.includes("foopy=1"), "Some cookies for me");
  },

  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  },
  null, false, false);
