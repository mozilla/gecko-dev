/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const gOverride = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

const DOMAIN_NAME = "example.com";
const HTTPS_FIRST = "dom.security.https_first";

registerCleanupFunction(function () {
  Services.prefs.clearUserPref(HTTPS_FIRST);
});

function waitForStatusChange(browser, expectedMessage) {
  return new Promise(resolve => {
    let listener = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),

      onStatusChange(webProgress, request, status, message) {
        info(`onStatusChange: ${message}`);
        // When we catch the correct message
        if (message === expectedMessage) {
          browser.webProgress.removeProgressListener(listener);
          resolve({ message });
        }
      },
      onProgressChange() {},
      onLocationChange() {},
      onSecurityChange() {},
      onStateChange() {},
      onContentBlockingEvent() {},
    };

    browser.webProgress.addProgressListener(
      listener,
      Ci.nsIWebProgress.NOTIFY_STATUS
    );
  });
}

add_task(async function test_domain_change() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.wait300msAfterTabSwitch", true],
      [HTTPS_FIRST, false],
    ],
  });

  gOverride.addIPOverride(DOMAIN_NAME, "127.0.0.1");
  let server = new HttpServer();
  server.start(-1);
  registerCleanupFunction(async () => {
    await server.stop();
    gOverride.clearOverrides();
  });

  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let serverURL = `http://${DOMAIN_NAME}:${server.identity.primaryPort}/`;
  server.identity.add("http", DOMAIN_NAME, server.identity.primaryPort);

  server.registerPathHandler("/", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html");
    const BODY = `testing..`;
    response.bodyOutputStream.write(BODY, BODY.length);
  });

  await SpecialPowers.pushPrefEnv({
    set: [["network.proxy.no_proxies_on", DOMAIN_NAME]],
  });

  let browser = gBrowser.selectedBrowser;
  let expectedMessage = `Transferring data from ${DOMAIN_NAME}…`;
  let statusPromise = waitForStatusChange(browser, expectedMessage);
  BrowserTestUtils.startLoadingURIString(browser, serverURL);
  let { message } = await statusPromise;
  is(message, expectedMessage, "Status message was received correctly");
});
