/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup() {
  const originalValue = Services.sysinfo.getProperty("hasUserNamespaces");
  Services.sysinfo
    .QueryInterface(Ci.nsIWritablePropertyBag2)
    .setPropertyAsBool("hasUserNamespaces", false);
  // Reset to original value at the end of the test
  registerCleanupFunction(() => {
    Services.sysinfo
      .QueryInterface(Ci.nsIWritablePropertyBag2)
      .setPropertyAsBool("hasUserNamespaces", originalValue);
  });
});

add_task(async function test_user_namespaces() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:support" },
    async function (browser) {
      const result = await SpecialPowers.spawn(browser, [], async function () {
        try {
          await ContentTaskUtils.waitForCondition(() => {
            const tbody = content.document.getElementById("sandbox-tbody");
            if (!tbody) {
              return false;
            }
            const tr = tbody.getElementsByTagName("tr");

            const expectedBackgroundColor = content.window.matchMedia(
              "(prefers-color-scheme: dark)"
            ).matches
              ? "rgb(105, 15, 34)"
              : "rgb(255, 232, 232)";

            return !![...tr]
              .filter(
                x =>
                  x.querySelector("th").dataset.l10nId === "has-user-namespaces"
              )
              .map(x => x.querySelector("td"))
              .filter(
                x =>
                  content
                    .getComputedStyle(x)
                    .getPropertyValue("background-color") ===
                  expectedBackgroundColor
              )
              .filter(
                x =>
                  x.querySelector("span").dataset.l10nId ===
                  "support-user-namespaces-unavailable"
              )
              .map(x => x.querySelector("a"))
              .filter(
                x =>
                  x.getAttribute("support-page") ===
                  "install-firefox-linux#w_install-firefox-from-mozilla-builds"
              ).length;
          }, "User Namespaces loaded and has correct properties");
        } catch (exception) {
          return false;
        }
        return true;
      });

      ok(
        result,
        "User Namespaces should be loaded to table, set to false, have red background color, and have support link"
      );
    }
  );
});
