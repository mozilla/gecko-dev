/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:support" }, async function(browser) {
    const strings = Services.strings.createBundle(
                      "chrome://global/locale/aboutSupport.properties");
    let allowedStates = [strings.GetStringFromName("found"),
                         strings.GetStringFromName("missing")];

    let keyLocationServiceGoogleStatus = await ContentTask.spawn(browser, null, async function() {
      let textBox = content.document.getElementById("key-location-service-google-box");
      await ContentTaskUtils.waitForCondition(() => textBox.textContent.trim(),
        "Google location service API key status loaded");
      return textBox.textContent;
    });
    ok(allowedStates.includes(keyLocationServiceGoogleStatus), "Google location service API key status shown");

    let keySafebrowsingGoogleStatus = await ContentTask.spawn(browser, null, async function() {
      let textBox = content.document.getElementById("key-safebrowsing-google-box");
      await ContentTaskUtils.waitForCondition(() => textBox.textContent.trim(),
        "Google Safebrowsing API key status loaded");
      return textBox.textContent;
    });
    ok(allowedStates.includes(keySafebrowsingGoogleStatus), "Google Safebrowsing API key status shown");

    let keyMozillaStatus = await ContentTask.spawn(browser, null, async function() {
      let textBox = content.document.getElementById("key-mozilla-box");
      await ContentTaskUtils.waitForCondition(() => textBox.textContent.trim(),
        "Mozilla API key status loaded");
      return textBox.textContent;
    });
    ok(allowedStates.includes(keyMozillaStatus), "Mozilla API key status shown");
  });
});
