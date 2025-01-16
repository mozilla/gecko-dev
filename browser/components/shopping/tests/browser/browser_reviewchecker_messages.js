/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NOT_ENOUGH_REVIEWS_TEST_URL =
  "https://example.com/Bad-Product/dp/N0T3NOUGHR";
const NOT_SUPPORTED_TEST_URL = "https://example.com/Bad-Product/dp/PAG3N0TSUP";
const UNPROCESSABLE_TEST_URL = "https://example.com/Bad-Product/dp/UNPR0C3SSA";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.main.tools", "aichat,reviewchecker,syncedtabs,history"],
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  registerCleanupFunction(async () => {
    SidebarController.hide();
  });
});

add_task(async function test_sidebar_error() {
  await BrowserTestUtils.withNewTab(BAD_PRODUCT_TEST_URL, async () => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(BAD_PRODUCT_TEST_URL);

    info("Verifying a generic error is shown.");
    await withReviewCheckerSidebar(async () => {
      let doc = content.document;
      let shoppingContainer =
        doc.querySelector("shopping-container").wrappedJSObject;

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
        "generic-error",
        "generic-error type should be correct"
      );
    });
  });
});

add_task(async function test_sidebar_analysis_status_page_not_supported() {
  // Product not supported status
  await BrowserTestUtils.withNewTab(NOT_SUPPORTED_TEST_URL, async () => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(NOT_SUPPORTED_TEST_URL);

    info("Verifying a generic error is shown.");
    await withReviewCheckerSidebar(async () => {
      let doc = content.document;
      let shoppingContainer =
        doc.querySelector("shopping-container").wrappedJSObject;

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
        "page-not-supported",
        "message type should be correct"
      );
    });
  });
});

add_task(async function test_sidebar_analysis_status_unprocessable() {
  // Unprocessable status
  await BrowserTestUtils.withNewTab(UNPROCESSABLE_TEST_URL, async () => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(UNPROCESSABLE_TEST_URL);

    info("Verifying a generic error is shown.");
    await withReviewCheckerSidebar(async () => {
      let doc = content.document;
      let shoppingContainer =
        doc.querySelector("shopping-container").wrappedJSObject;

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
        "generic-error",
        "message type should be correct"
      );
    });
  });
});

add_task(async function test_sidebar_analysis_status_not_enough_reviews() {
  // Not enough reviews status
  await BrowserTestUtils.withNewTab(NOT_ENOUGH_REVIEWS_TEST_URL, async () => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(NOT_ENOUGH_REVIEWS_TEST_URL);

    info("Verifying a generic error is shown.");
    await withReviewCheckerSidebar(async () => {
      let doc = content.document;
      let shoppingContainer =
        doc.querySelector("shopping-container").wrappedJSObject;

      ok(
        shoppingContainer.shoppingMessageBarEl,
        "Got shopping-message-bar element"
      );
      is(
        shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
        "not-enough-reviews",
        "message type should be correct"
      );
    });
  });
});
