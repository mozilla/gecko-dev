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

add_setup(() => {
  let mockCid = MockRegistrar.register(
    "@mozilla.org/alerts-service;1",
    mockAlertsService
  );

  let controller = new AbortController();
  let { signal } = controller;

  registerCleanupFunction(() => {
    MockRegistrar.unregister(mockCid);
    controller.abort();
  });

  BrowserTestUtils.concealWindow(window, { signal });
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
