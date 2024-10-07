/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure the buttons at the buttom of the HTTPS-Only error page do not get
// displayed in an iframe (Bug 1909396).

add_task(async function test_iframe_buttons() {
  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/dom/security/test/https-only/file_iframe_buttons.html",
    async function (browser) {
      await SpecialPowers.pushPrefEnv({
        set: [["dom.security.https_only_mode", true]],
      });

      await SpecialPowers.spawn(browser, [], async function () {
        const iframe = content.document.getElementById("iframe");
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        iframe.src = "http://nocert.example.com";

        await ContentTaskUtils.waitForCondition(
          () => iframe.contentWindow.document.readyState === "interactive",
          "Iframe error page should have loaded"
        );

        ok(
          !!iframe.contentWindow.document.getElementById("explanation-iframe"),
          "#explanation-iframe should exist"
        );

        is(
          iframe.contentWindow.document
            .getElementById("explanation-iframe")
            .getAttribute("hidden"),
          null,
          "#explanation-iframe should not be hidden"
        );

        for (const id of ["explanation-continue", "goBack", "openInsecure"]) {
          is(
            iframe.contentWindow.document.getElementById(id),
            null,
            `#${id} should have been removed`
          );
        }
      });
    }
  );
});
