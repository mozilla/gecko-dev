/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/* eslint-disable @microsoft/sdl/no-insecure-url */

// This test does the following:
// 1. Go to http://example.com/..., which should get upgraded
// 2. The server redirects the upgraded https://example.com/...  back to http://example.com/...
// 3. We detect the site doesn't support HTTPS and add an automatic exception
// 4. We load http://example.com/..., which redirects to http://example.org/... (a different origin)
// 5. We **should** upgrade this load, because we should clear our exemption on redirects

add_task(async function test_redirect_exemption_clearing() {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    BrowserTestUtils.startLoadingURIString(
      browser,
      "http://example.com/browser/dom/security/test/https-first/file_redirect_exemption_clearing.sjs"
    );
    await BrowserTestUtils.browserLoaded(browser);

    info(`Browser stopped loading on ${browser.contentPrincipal.asciiSpec}`);

    is(
      browser.contentPrincipal.scheme,
      "https",
      "Site after redirect should be upgraded"
    );

    let perm = Services.perms.testExactPermissionFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "http://example.com"
      ),
      "https-only-load-insecure"
    );
    is(
      perm,
      Ci.nsIHttpsOnlyModePermission.HTTPSFIRST_LOAD_INSECURE_ALLOW,
      "An automatic exception should have been added for the source site"
    );
  });
});
