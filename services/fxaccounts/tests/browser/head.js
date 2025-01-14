/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Waits for the next load to complete in any browser or the given browser.
 * If a <tabbrowser> is given it waits for a load in any of its browsers.
 *
 * @return promise
 */
function waitForDocLoadComplete(aBrowser = gBrowser) {
  return new Promise(resolve => {
    let listener = {
      onStateChange(webProgress, req, flags, status) {
        let docStop =
          Ci.nsIWebProgressListener.STATE_IS_NETWORK |
          Ci.nsIWebProgressListener.STATE_STOP;
        info(
          "Saw state " +
            flags.toString(16) +
            " and status " +
            status.toString(16)
        );

        // When a load needs to be retargetted to a new process it is cancelled
        // with NS_BINDING_ABORTED so ignore that case
        if ((flags & docStop) == docStop && status != Cr.NS_BINDING_ABORTED) {
          aBrowser.removeProgressListener(this);
          waitForDocLoadComplete.listeners.delete(this);

          let chan = req.QueryInterface(Ci.nsIChannel);
          info("Browser loaded " + chan.originalURI.spec);
          resolve();
        }
      },
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };
    aBrowser.addProgressListener(listener);
    waitForDocLoadComplete.listeners.add(listener);
    info("Waiting for browser load");
  });
}

function setupMockAlertsService(expectedObj) {
  const alertsService = {
    showAlertNotification: (
      image,
      title,
      body,
      clickable,
      cookie,
      clickCallback
    ) => {
      // We need to invoke the event handler ourselves.
      clickCallback(null, "alertshow", null);

      // check the expectations, if passed in
      if (expectedObj) {
        expectedObj.title && Assert.equal(title, expectedObj.title);
        expectedObj.body && Assert.equal(body, expectedObj.body);
      }

      // We are invoking the event handler ourselves directly.
      clickCallback(null, "alertclickcallback", null);
      clickCallback(null, "alertfinished", null);
    },
  };
  const { AccountsGlue } = ChromeUtils.importESModule(
    "resource:///modules/AccountsGlue.sys.mjs"
  );
  const wrappedService = { wrappedJSObject: alertsService };
  AccountsGlue.observe(
    wrappedService,
    "browser-glue-test",
    "mock-alerts-service"
  );
}

// Keep a set of progress listeners for waitForDocLoadComplete() to make sure
// they're not GC'ed before we saw the page load.
waitForDocLoadComplete.listeners = new Set();
registerCleanupFunction(() => waitForDocLoadComplete.listeners.clear());
