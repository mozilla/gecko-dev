// Bug 1725026 - HTTPS Only Mode - Test if a load triggered by a user gesture
// gesture can be upgraded to HTTPS successfully.

"use strict";

const testPathUpgradeable = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const kTestURI = testPathUpgradeable + "file_user_gesture.sjs";

add_task(async function () {
  // Enable HTTPS-Only Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  for (const buttonId of ["directButton", "redirectButton"]) {
    info(
      {
        directButton: "Testing direct upgrade after user gesture",
        redirectButton: "Testing upgrade after user gesture and redirect",
      }[buttonId]
    );

    await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
      let loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
      // 1. Upgrade a page to https://
      BrowserTestUtils.startLoadingURIString(browser, kTestURI);
      await loaded;
      await ContentTask.spawn(browser, [buttonId], async buttonId => {
        ok(
          content.document.location.href.startsWith("https://"),
          "Should be https"
        );

        // 2. Trigger a load by clicking button.
        // The scheme of the link url is `http` and the load should be able to
        // upgraded to `https` because of HTTPS-only mode.
        let button = content.document.getElementById(buttonId);
        await EventUtils.synthesizeMouseAtCenter(
          button,
          { type: "mousedown" },
          content
        );
        await EventUtils.synthesizeMouseAtCenter(
          button,
          { type: "mouseup" },
          content
        );
      });
      await BrowserTestUtils.browserLoaded(browser, false, null, true);
      info(browser.currentURI.spec);
      is(browser.currentURI.scheme, "https", "Should be https");
    });
  }
});
