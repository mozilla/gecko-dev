/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

"use strict";

const ABOUT_BLANK = "about:blank";
const CONTENT_PAGE = "https://example.com";

ChromeUtils.defineESModuleGetters(this, {
  ContentTaskUtils: "resource://testing-common/ContentTaskUtils.sys.mjs",
});

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
  await SidebarController.initializeUIState({
    launcherVisible: true,
    command: "",
  });
});

add_task(async function test_integrated_sidebar() {
  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    const { document } = browser.ownerGlobal;
    let sidebar = document.querySelector("sidebar-main");
    await sidebar.updateComplete;
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
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

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
    await reviewCheckerSidebarUpdated(OTHER_PRODUCT_TEST_URL);

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
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

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
    await reviewCheckerSidebarUpdated(OTHER_PRODUCT_TEST_URL);

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

add_task(async function test_integrated_sidebar_close() {
  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function () {
    let sandbox = sinon.createSandbox();
    // Stub SidebarController.hide as actually closing the sidebar
    // will not allow withReviewCheckerSidebar to finish.
    let hideStub = sandbox.stub(SidebarController, "hide");
    await SidebarController.show("viewReviewCheckerSidebar");
    await reviewCheckerSidebarUpdated(CONTENT_PAGE);

    ok(SidebarController.isOpen, "Sidebar is open");

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      let closeButtonEl = await ContentTaskUtils.waitForCondition(
        () => shoppingContainer.closeButtonEl,
        "close button is present."
      );
      closeButtonEl.click();
    });

    Assert.ok(
      hideStub.calledOnce,
      "SidebarController.hide() is called to close the sidebar."
    );

    sandbox.restore();
  });
});

add_task(async function test_integrated_sidebar_empty_states() {
  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    await SidebarController.show("viewReviewCheckerSidebar");
    await reviewCheckerSidebarUpdated(CONTENT_PAGE);

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
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
        !shoppingContainer.isProductPage,
        "Current page is not a product page"
      );
      Assert.ok(
        shoppingContainer.isSupportedSite,
        "Current page is a supported site"
      );
    });

    BrowserTestUtils.startLoadingURIString(browser, PRODUCT_TEST_URL);
    await BrowserTestUtils.browserLoaded(browser);
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      await shoppingContainer.updateComplete;
      await ContentTaskUtils.waitForCondition(
        () => typeof shoppingContainer.isProductPage !== "undefined",
        "isProductPage is set."
      );
      Assert.ok(
        shoppingContainer.isProductPage,
        "Current page is a product page"
      );
      Assert.ok(
        !shoppingContainer.isSupportedSite,
        "Current page is not a supported site"
      );
    });

    BrowserTestUtils.startLoadingURIString(browser, "about:newtab");
    await BrowserTestUtils.browserLoaded(browser);

    // Set review checker to show because it will be hidden by default
    // on an unsupported page with auto-close enabled
    await SidebarController.show("viewReviewCheckerSidebar");
    await reviewCheckerSidebarUpdated("about:newtab");

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
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
        !shoppingContainer.isProductPage,
        "Current page is not a product page"
      );
      Assert.ok(
        !shoppingContainer.isSupportedSite,
        "Current page is not a supported site"
      );
    });
  });
});

add_task(async function test_sidebar_navigation() {
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async browser => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);
    info("Verifying product info for initial product.");
    await verifyReviewCheckerSidebarProductInfo({
      productURL: PRODUCT_TEST_URL,
      adjustedRating: "4.1",
      letterGrade: "B",
    });

    // Navigate the browser from the parent:
    let loadedPromise = Promise.all([
      BrowserTestUtils.browserLoaded(browser, false, OTHER_PRODUCT_TEST_URL),
      reviewCheckerSidebarUpdated(OTHER_PRODUCT_TEST_URL),
    ]);
    BrowserTestUtils.startLoadingURIString(browser, OTHER_PRODUCT_TEST_URL);
    info("Loading another product.");
    await loadedPromise;
    info("Verifying another product.");
    await verifyReviewCheckerSidebarProductInfo({
      productURL: OTHER_PRODUCT_TEST_URL,
      adjustedRating: "1",
      letterGrade: "F",
    });

    // Navigate to a non-product URL:
    loadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      "https://example.com/1"
    );
    BrowserTestUtils.startLoadingURIString(browser, "https://example.com/1");
    info("Go to a non-product.");
    await loadedPromise;
    await reviewCheckerSidebarUpdated("https://example.com/1");
    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );

      Assert.ok(shoppingContainer, "Review Checker is loaded");

      await shoppingContainer.updateComplete;
      Assert.ok(
        !shoppingContainer.isProductPage,
        "Current page is not a product page"
      );
      Assert.ok(
        shoppingContainer.isSupportedSite,
        "Current page is a supported site"
      );
    });

    // Navigate using pushState:
    loadedPromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      PRODUCT_TEST_URL
    );
    info("Navigate to the first product using pushState.");
    await SpecialPowers.spawn(browser, [PRODUCT_TEST_URL], urlToUse => {
      content.history.pushState({}, null, urlToUse);
    });
    info("Waiting to load first product again.");
    await loadedPromise;
    info("Waiting for the sidebar to have updated.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    info("Waiting to verify the first product a second time.");
    await verifyReviewCheckerSidebarProductInfo({
      productURL: PRODUCT_TEST_URL,
      adjustedRating: "4.1",
      letterGrade: "B",
    });

    // Navigate to a product URL with query params:
    loadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      PRODUCT_TEST_URL + "?th=1"
    );
    // Navigate to the same product, but with a th=1 added.
    BrowserTestUtils.startLoadingURIString(browser, PRODUCT_TEST_URL + "?th=1");
    // When just comparing URLs product info would be cleared out,
    // but when comparing the parsed product ids, we do nothing as the product
    // has not changed.
    info("Verifying product has not changed before load.");
    await verifyReviewCheckerSidebarProductInfo({
      productURL: PRODUCT_TEST_URL,
      adjustedRating: "4.1",
      letterGrade: "B",
    });
    // Wait for the page to load, but don't wait for the sidebar to update so
    // we can be sure we still have the previous product info.
    await loadedPromise;
    info("Verifying product has not changed after load.");
    await verifyReviewCheckerSidebarProductInfo({
      productURL: PRODUCT_TEST_URL,
      adjustedRating: "4.1",
      letterGrade: "B",
    });
  });
});

add_task(async function test_close_sidebar_after_opt_out() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.optedIn", 1]],
  });
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async () => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );

      Assert.ok(shoppingContainer, "Review Checker is loaded");

      await shoppingContainer.updateComplete;

      let shoppingSettings = shoppingContainer.settingsEl;
      Assert.ok(shoppingSettings, "Got the shopping-settings element");

      let optOutButton = shoppingSettings.optOutButtonEl;
      Assert.ok(optOutButton, "There should be an opt-out button");

      optOutButton.click();
    });

    let rcSidebarPanelOpen =
      SidebarController.isOpen &&
      SidebarController.currentID == "viewReviewCheckerSidebar";
    Assert.ok(
      !rcSidebarPanelOpen,
      "Review Checker panel is closed after pressing the opt-out button"
    );
  });

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the shopping-message-bar component does not appear when switching to another page
 * with an analysis in progress
 */
add_task(
  async function test_inprogress_message_does_not_appear_on_navigation() {
    await BrowserTestUtils.withNewTab(
      NEEDS_ANALYSIS_TEST_URL,
      async browser => {
        await SidebarController.show("viewReviewCheckerSidebar");
        info("Waiting for sidebar to update.");
        await reviewCheckerSidebarUpdated(NEEDS_ANALYSIS_TEST_URL);

        await withReviewCheckerSidebar(async () => {
          let shoppingContainer = await ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector("shopping-container")
                ?.wrappedJSObject,
            "Review Checker is loaded."
          );

          /**
           * Let's ensure the inprogress message-bar is visible so that we can reliability test its
           * visibility later in the test
           */
          let messageBarVisiblePromise = ContentTaskUtils.waitForCondition(
            () => {
              return (
                !!shoppingContainer.shoppingMessageBarEl &&
                ContentTaskUtils.isVisible(
                  shoppingContainer.shoppingMessageBarEl
                )
              );
            },
            "Waiting for shopping-message-bar to be visible"
          );

          info("Pressing analysis button");
          let analysisButton =
            shoppingContainer.unanalyzedProductEl.analysisButtonEl;
          analysisButton.click();

          await messageBarVisiblePromise;
        });

        // Load a new page
        let loadedPromise = BrowserTestUtils.browserLoaded(
          browser,
          false,
          "https://example.com/1"
        );

        BrowserTestUtils.startLoadingURIString(
          browser,
          "https://example.com/1"
        );
        info("Loading non-product page");
        await loadedPromise;
        await reviewCheckerSidebarUpdated("https://example.com/1");

        await withReviewCheckerSidebar(
          async args => {
            const [NEEDS_ANALYSIS_TEST_URL] = args;

            let actor =
              content.windowGlobalChild.getExistingActor("ReviewChecker");
            Assert.ok(actor, "Got ReviewCheckerChild actor");

            info(
              "Calling updateAnalysisStatus to pretend it resolved late from the previous URL after loading a new one"
            );
            await actor.updateAnalysisStatus(
              { status: "pending" },
              NEEDS_ANALYSIS_TEST_URL
            );

            let shoppingContainer = await ContentTaskUtils.waitForCondition(
              () =>
                content.document.querySelector("shopping-container")
                  ?.wrappedJSObject,
              "Review Checker is loaded."
            );

            // Now, let's ensure the inprogress message-bar is never visible in Review Checker.
            let messageBarVisiblePromise = ContentTaskUtils.waitForCondition(
              () => {
                return (
                  !!shoppingContainer.shoppingMessageBarEl &&
                  ContentTaskUtils.isVisible(
                    shoppingContainer.shoppingMessageBarEl
                  )
                );
              },
              "Waiting for shopping-message-bar to be visible"
            );

            // ContentTaskUtils.waitForCondition error message will include "timed out" since it couldn't find the shopping-message-bar.
            await Assert.rejects(
              messageBarVisiblePromise,
              /timed out/,
              "shopping-message-bar for in-progress analysis should never be visible"
            );

            Assert.ok(
              !shoppingContainer.shoppingMessageBarEl,
              "The shopping message bar element does not exist on non-PDP"
            );
          },
          [NEEDS_ANALYSIS_TEST_URL]
        );
      }
    );
  }
);

/**
 * Tests that the auto-close toggle component is rendered as expected
 * and that auto-close toggle UI and browser.shopping.experience2023.autoClose.userEnabled
 * pref update correctly when the toggle is clicked.
 */
add_task(async function test_settings_auto_close_toggle() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.autoClose.userEnabled", true]],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function () {
    await SidebarController.show("viewReviewCheckerSidebar");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded"
      );

      await shoppingContainer.updateComplete;

      let shoppingSettings = shoppingContainer.settingsEl;
      ok(shoppingSettings, "Got the shopping-settings element");

      let autoCloseToggle = shoppingSettings.autoCloseToggleEl;
      ok(autoCloseToggle, "There should be an auto-close toggle");
      ok(
        autoCloseToggle.hasAttribute("pressed"),
        "Toggle should have enabled state"
      );

      let toggleStateChangePromise = ContentTaskUtils.waitForCondition(() => {
        return !autoCloseToggle.hasAttribute("pressed");
      }, "Waiting for auto-close toggle state to be disabled");
      let autoCloseUserEnabledPromise = ContentTaskUtils.waitForEvent(
        content.document,
        "autoCloseEnabledByUserChanged"
      );
      let autoClosePrefChange = ContentTaskUtils.waitForCondition(
        () =>
          !SpecialPowers.getBoolPref(
            "browser.shopping.experience2023.autoClose.userEnabled"
          ),
        "Auto-close pref should be false, but isn't"
      );

      autoCloseToggle.click();

      Promise.all([
        await toggleStateChangePromise,
        await autoCloseUserEnabledPromise,
        await autoClosePrefChange,
      ]);

      // Make sure that clicking the toggle updates the pref
      ok(
        !SpecialPowers.getBoolPref(
          "browser.shopping.experience2023.autoClose.userEnabled"
        ),
        "autoOpen.userEnabled pref should be false"
      );
    });
    await Services.fog.testFlushAllChildren();
    let toggledEvents =
      Glean.shopping.surfaceAutoCloseSettingToggled.testGetValue();

    Assert.equal(toggledEvents.length, 1);
    Assert.equal(toggledEvents[0].category, "shopping");
    Assert.equal(toggledEvents[0].name, "surface_auto_close_setting_toggled");
  });
  await SpecialPowers.popPrefEnv();
});

/* Tests that the auto-close toggle UI and browser.shopping.experience2023.autoClose.userEnabled
 *  pref are updated when auto-close pref is changed outside of toggle action.
 */
add_task(async function test_settings_auto_close_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.autoClose.userEnabled", true]],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function () {
    await SidebarController.show("viewReviewCheckerSidebar");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded"
      );

      await shoppingContainer.updateComplete;

      let shoppingSettings = shoppingContainer.settingsEl;
      ok(shoppingSettings, "Got the shopping-settings element");

      let autoCloseToggle = shoppingSettings.autoCloseToggleEl;
      ok(autoCloseToggle, "There should be an auto-close toggle");
      ok(
        autoCloseToggle.hasAttribute("pressed"),
        "Toggle should initially have enabled state"
      );

      // Confirm that pref update updates the UI and sends autoClose event
      let toggleStateChangePromise = ContentTaskUtils.waitForCondition(() => {
        return !autoCloseToggle.hasAttribute("pressed");
      }, "Waiting for auto-close toggle state to be disabled");
      let autoCloseUserEnabledPromise = ContentTaskUtils.waitForEvent(
        content.document,
        "autoCloseEnabledByUserChanged"
      );

      await SpecialPowers.popPrefEnv();

      await SpecialPowers.pushPrefEnv({
        set: [["browser.shopping.experience2023.autoClose.userEnabled", false]],
      });

      Promise.all([
        await toggleStateChangePromise,
        await autoCloseUserEnabledPromise,
      ]);
    });
  });
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that about:blank is treated as an unsupported site
 * and renders the loading state as a result.
 */
add_task(async function test_about_blank_unsupported() {
  await BrowserTestUtils.withNewTab(ABOUT_BLANK, async function () {
    await SidebarController.show("viewReviewCheckerSidebar");

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded"
      );

      await shoppingContainer.updateComplete;

      let loadingState = await ContentTaskUtils.waitForCondition(
        () => shoppingContainer.loadingEl,
        "Loading state found"
      );

      Assert.ok(loadingState, "Loading state found");
    });
  });

  SidebarController.hide();
});
