/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

"use strict";

const CONTENT_PAGE = "https://example.com";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["browser.shopping.experience2023.shoppingSidebar", false],
      ["sidebar.main.tools", "aichat,reviewchecker,syncedtabs,history"],
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  registerCleanupFunction(async () => {
    SidebarController.hide();
  });
});

add_task(async function test_integrated_sidebar() {
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

    await SpecialPowers.pushPrefEnv({
      set: [["browser.shopping.experience2023.integratedSidebar", false]],
    });

    await TestUtils.waitForCondition(
      () =>
        !sidebar.shadowRoot.querySelector(
          "moz-button[view=viewReviewCheckerSidebar]"
        ),
      "Review Checker Button is removed."
    );
    ok(!SidebarController.isOpen, "Sidebar is closed");
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_integrated_sidebar_renders_product() {
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function (browser) {
    await SidebarController.show("viewReviewCheckerSidebar");

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      await shoppingContainer.updateComplete;

      let reviewReliability = await ContentTaskUtils.waitForCondition(
        () => shoppingContainer.reviewReliabilityEl,
        "review-reliability is loaded."
      );

      ok(reviewReliability, "The review-reliability element exists");

      Assert.equal(
        reviewReliability.getAttribute("letter"),
        "B",
        "The grade is correct."
      );
    });

    let loadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      OTHER_PRODUCT_TEST_URL
    );
    BrowserTestUtils.startLoadingURIString(browser, OTHER_PRODUCT_TEST_URL);
    info("Loading another product.");
    await loadedPromise;

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      await shoppingContainer.updateComplete;

      let reviewReliability = await ContentTaskUtils.waitForCondition(
        () => shoppingContainer.reviewReliabilityEl,
        "review-reliability is loaded."
      );

      ok(reviewReliability, "The review-reliability element exists");

      Assert.equal(
        reviewReliability.getAttribute("letter"),
        "F",
        "The grade is correct."
      );
    });

    SidebarController.hide();
  });
});

add_task(async function test_integrated_sidebar_updates_on_tab_switch() {
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function () {
    let newProductTab = BrowserTestUtils.addTab(
      gBrowser,
      OTHER_PRODUCT_TEST_URL
    );
    let newProductBrowser = newProductTab.linkedBrowser;
    await BrowserTestUtils.browserLoaded(
      newProductBrowser,
      false,
      OTHER_PRODUCT_TEST_URL
    );

    await SidebarController.show("viewReviewCheckerSidebar");

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      await shoppingContainer.updateComplete;

      let reviewReliability = await ContentTaskUtils.waitForCondition(
        () => shoppingContainer.reviewReliabilityEl,
        "review-reliability is loaded."
      );

      ok(reviewReliability, "The review-reliability element exists");

      Assert.equal(
        reviewReliability.getAttribute("letter"),
        "B",
        "The grade is correct."
      );
    });

    await BrowserTestUtils.switchTab(gBrowser, newProductTab);

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      await shoppingContainer.updateComplete;

      let reviewReliability = await ContentTaskUtils.waitForCondition(
        () => shoppingContainer.reviewReliabilityEl,
        "review-reliability is loaded."
      );

      ok(reviewReliability, "The review-reliability element exists");

      Assert.equal(
        reviewReliability.getAttribute("letter"),
        "F",
        "The grade is correct."
      );
    });

    SidebarController.hide();

    await BrowserTestUtils.removeTab(newProductTab);
  });
});
