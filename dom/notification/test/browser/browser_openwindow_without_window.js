const PAGE_URI =
  "https://example.com/browser/dom/notification/test/browser/empty.html";
const OPEN_URI =
  "https://example.com/browser/dom/notification/test/browser/file_openwindow.html";

let { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

let callListener = () => {
  throw new Error("callListener is unexpectedly called before showAlert");
};

let mockAlertsService = {
  showAlert(alert, alertListener) {
    ok(true, "Showing alert");
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(function () {
      alertListener.observe(null, "alertshow", null);
    }, 100);
    callListener = () =>
      alertListener.observe(null, "alertclickcallback", null);
  },

  QueryInterface: ChromeUtils.generateQI(["nsIAlertsService"]),

  createInstance(aIID) {
    return this.QueryInterface(aIID);
  },
};

// Stolen from https://searchfox.org/mozilla-central/source/browser/base/content/test/popups/browser_popup_close_main_window.js
// When calling this function, the main window where the test runs will be
// hidden from various APIs, so that they won't be able to find it. This makes
// it possible to test some behaviors when no browser window is present.
// See bug 1972344 to move this function to BrowserTestUtils.
function concealMainWindow() {
  info("Concealing main window.");
  let oldWinType = document.documentElement.getAttribute("windowtype");
  // Check if we've already done this to allow calling multiple times:
  if (oldWinType != "navigator:testrunner") {
    // Make the main test window not count as a browser window any longer
    document.documentElement.setAttribute("windowtype", "navigator:testrunner");

    registerCleanupFunction(() => {
      info("Unconcealing the main window in the cleanup phase.");
      document.documentElement.setAttribute("windowtype", oldWinType);
    });
  }
}

add_setup(() => {
  let mockCid = MockRegistrar.register(
    "@mozilla.org/alerts-service;1",
    mockAlertsService
  );

  registerCleanupFunction(() => {
    MockRegistrar.unregister(mockCid);
  });

  concealMainWindow();
});

for (let permanentPbm of [false, true]) {
  add_task(async function () {
    info(`Test with PBM: ${permanentPbm}`);

    await SpecialPowers.pushPrefEnv({
      set: [["browser.privatebrowsing.autostart", permanentPbm]],
    });

    let win = await BrowserTestUtils.openNewBrowserWindow();
    let browser = win.gBrowser.selectedBrowser;

    BrowserTestUtils.startLoadingURIString(browser, PAGE_URI);

    await BrowserTestUtils.browserLoaded(browser);

    // Register a service worker and show a notification
    await SpecialPowers.spawn(browser, [], async () => {
      await SpecialPowers.pushPermissions([
        {
          type: "desktop-notification",
          allow: SpecialPowers.Services.perms.ALLOW_ACTION,
          context: content.document,
        },
      ]);

      // Registration of the SW
      const swr = await content.navigator.serviceWorker.register(
        "file_openwindow.serviceworker.js"
      );

      // Activation
      await content.navigator.serviceWorker.ready;

      // Ask for an openWindow.
      await swr.showNotification("testPopup");
    });

    registerCleanupFunction(() => SpecialPowers.removeAllServiceWorkerData());

    // Now close the current window
    await BrowserTestUtils.closeWindow(win);

    let newWinPromise = BrowserTestUtils.waitForNewWindow({
      url: OPEN_URI,
    });

    // Simulate clicking the notification
    callListener();

    // See it can open a new window
    let newWin = await newWinPromise;
    ok(true, "Should get the window");

    is(
      PrivateBrowsingUtils.isWindowPrivate(newWin),
      permanentPbm,
      "PBM state of the new window should match the existing PBM mode"
    );

    await SpecialPowers.spawn(newWin.gBrowser.selectedBrowser, [], async () => {
      if (!content.wrappedJSObject.promise) {
        throw new Error("Not a promise");
      }
      await content.wrappedJSObject.promise;
    });
    ok(true, "Should be able to post to the resulting WindowClient");

    newWin.close();
  });
}
