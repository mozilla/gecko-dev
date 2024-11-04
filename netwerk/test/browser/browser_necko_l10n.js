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

function observeStatusLabel(statusLabel, expectedValues) {
  // Verify if all values from the expectedValues set are shown in statusLabel
  return new Promise(resolve => {
    let prevValue = statusLabel.value;
    if (prevValue) {
      expectedValues.delete(prevValue);
    }

    const observer = new MutationObserver(mutations => {
      for (let mutation of mutations) {
        if (mutation.attributeName === "value") {
          if (statusLabel.value && statusLabel.value !== prevValue) {
            expectedValues.delete(statusLabel.value);

            if (expectedValues.size === 0) {
              ok(true, "All expected values were matched");
              observer.disconnect();
              resolve();
            }
            prevValue = statusLabel.value;
          }
        }
      }
    });

    observer.observe(statusLabel, { attributes: true });
    return () => {
      observer.disconnect();
      is(expectedValues.size, 0, "Not all expected values were matched");
      resolve();
    };
  });
}

add_task(async function test_domain_change() {
  await SpecialPowers.pushPrefEnv({
    set: [[HTTPS_FIRST, false]],
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

  let expectedValues = new Set([
    DOMAIN_NAME,
    `Transferring data from ${DOMAIN_NAME}…`,
  ]);

  let statusLabel = document.getElementById("statuspanel-label");
  let statusPromise = observeStatusLabel(statusLabel, expectedValues);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, serverURL);
  await statusPromise;
});
