/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PAGE = "https://example.com";

add_task(async function test_integrated_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["sidebar.main.tools", "aichat,reviewchecker,syncedtabs,history"],
    ],
  });

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    const { document } = browser.ownerGlobal;
    let sidebar = document.querySelector("sidebar-main");
    let reviewCheckerButton = await TestUtils.waitForCondition(
      () =>
        sidebar.shadowRoot.querySelector(
          "moz-button[view=viewReviewCheckerSidebar]"
        ),
      "Review Checker Button is added."
    );
    ok(
      BrowserTestUtils.isVisible(reviewCheckerButton),
      "Review Checker Button should be visible"
    );

    reviewCheckerButton.click();

    ok(SidebarController.isOpen, "Sidebar is open");

    Assert.equal(
      SidebarController.currentID,
      "viewReviewCheckerSidebar",
      "Sidebar should have opened to the review checker"
    );

    Services.prefs.setBoolPref(
      "browser.shopping.experience2023.integratedSidebar",
      false
    );

    await TestUtils.waitForCondition(
      () =>
        !sidebar.shadowRoot.querySelector(
          "moz-button[view=viewReviewCheckerSidebar]"
        ),
      "Review Checker Button is removed."
    );
    ok(!SidebarController.isOpen, "Sidebar is closed");
  });

  Services.prefs.clearUserPref(
    "browser.shopping.experience2023.integratedSidebar"
  );
  await SpecialPowers.popPrefEnv();
});
