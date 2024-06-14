// This test verifies that Firefox correctly upgrades an HTTP request to HTTPS
// when the request's domain name matches network.dns.mock_HTTPS_RR_domain.

"use strict";

const testPathUpgradeable = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org"
);

const kTestURI = testPathUpgradeable + "dummy.html";

add_task(async function () {
  // Set the mock_HTTPS_RR_domain and tell necko to use HTTPS RR.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.dns.mock_HTTPS_RR_domain", "example.org"],
      ["network.dns.force_use_https_rr", true],
    ],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    // The page should be upgraded to HTTPS.
    BrowserTestUtils.startLoadingURIString(browser, kTestURI);
    await loaded;
    await ContentTask.spawn(browser, {}, async () => {
      ok(
        content.document.location.href.startsWith("https://"),
        "Should be https"
      );
    });
  });
});
