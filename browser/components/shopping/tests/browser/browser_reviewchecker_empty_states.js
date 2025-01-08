/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

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

add_task(
  async function test_integrated_sidebar_empty_state_opted_in_supported_site() {
    await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (_browser) {
      await SidebarController.show("viewReviewCheckerSidebar");
      await reviewCheckerSidebarUpdated(CONTENT_PAGE);

      await withReviewCheckerSidebar(async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector("shopping-container")
              ?.wrappedJSObject,
          "Review Checker is loaded."
        );

        Assert.ok(shoppingContainer, "Review Checker is loaded");

        await shoppingContainer.updateComplete;
        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.isProductPage !== "undefined",
          "isProductPage is set."
        );
        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.isSupportedSite !== "undefined",
          "isSupportedSite is set."
        );

        Assert.ok(
          shoppingContainer.emptyStateImgEl,
          "Empty state should have an image"
        );
        Assert.ok(
          shoppingContainer.emptyStateHeaderEl,
          "Empty state should have a header"
        );
        Assert.ok(
          shoppingContainer.emptyStateTextEl,
          "Empty state should have text"
        );
        Assert.ok(
          !shoppingContainer.emptyStateSupportedListEl,
          "Empty state should not have a list of sites for a supported site"
        );
        Assert.ok(
          shoppingContainer.analysisExplainerEl,
          "Empty state should show the analysis-explainer for a supported site"
        );
        Assert.ok(
          shoppingContainer.settingsEl,
          "Empty state should have the settings card"
        );
        Assert.equal(
          shoppingContainer.containerContentEl?.className,
          "is-empty-state",
          "There should be a classname for empty state so that styles are applied"
        );
      });
    });
  }
);

add_task(
  async function test_integrated_sidebar_empty_state_opted_in_not_supported_site() {
    await BrowserTestUtils.withNewTab("about:about", async function (_browser) {
      await SidebarController.show("viewReviewCheckerSidebar");
      await reviewCheckerSidebarUpdated("about:about");

      await withReviewCheckerSidebar(async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector("shopping-container")
              ?.wrappedJSObject,
          "Review Checker is loaded."
        );
        await shoppingContainer.updateComplete;

        Assert.ok(shoppingContainer, "Review Checker is loaded");

        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.isProductPage !== "undefined",
          "isProductPage is set."
        );
        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.isSupportedSite !== "undefined",
          "isSupportedSite is set."
        );

        Assert.ok(
          shoppingContainer.emptyStateImgEl,
          "Empty state should have an image"
        );
        Assert.ok(
          shoppingContainer.emptyStateHeaderEl,
          "Empty state should have a header"
        );
        Assert.ok(
          shoppingContainer.emptyStateTextEl,
          "Empty state should have text"
        );
        Assert.ok(
          shoppingContainer.emptyStateSupportedListEl,
          "Empty state should have a list of sites for an unsupported site"
        );
        Assert.ok(
          !shoppingContainer.analysisExplainerEl,
          "Empty state should not show the analysis-explainer for an unsupported site"
        );
        Assert.ok(
          shoppingContainer.settingsEl,
          "Empty state should have the settings card"
        );
        Assert.equal(
          shoppingContainer.containerContentEl?.className,
          "is-empty-state",
          "There should be a classname for empty state so that styles are applied"
        );
      });
    });
  }
);

add_task(async function test_integrated_sidebar_no_empty_state_opted_in_pdp() {
  await BrowserTestUtils.withNewTab(
    PRODUCT_TEST_URL,
    async function (_browser) {
      await SidebarController.show("viewReviewCheckerSidebar");
      await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

      await withReviewCheckerSidebar(async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector("shopping-container")
              ?.wrappedJSObject,
          "Review Checker is loaded."
        );
        await shoppingContainer.updateComplete;

        Assert.ok(shoppingContainer, "Review Checker is loaded");

        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.isProductPage !== "undefined",
          "isProductPage is set."
        );
        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.isSupportedSite !== "undefined",
          "isSupportedSite is set."
        );

        Assert.ok(
          !shoppingContainer.emptyStateImgEl,
          "There should be no empty state image"
        );
        Assert.ok(
          !shoppingContainer.emptyStateHeaderEl,
          "There should be no empty state header"
        );
        Assert.ok(
          !shoppingContainer.emptyStateTextEl,
          "There should be no empty state text"
        );
        Assert.ok(
          !shoppingContainer.emptyStateSupportedListEl,
          "There should be no empty state list of supported sites"
        );
        Assert.equal(
          shoppingContainer.shadowRoot.querySelectorAll("analysis-explainer")
            .length,
          1,
          "There should be only one analysis-explainer"
        );
        Assert.equal(
          shoppingContainer.shadowRoot.querySelectorAll("shopping-settings")
            .length,
          1,
          "There should be only one shopping-settings"
        );
        Assert.notEqual(
          shoppingContainer.containerContentEl?.className,
          "is-empty-state",
          "There should be no classname for empty state"
        );
      });
    }
  );
});
