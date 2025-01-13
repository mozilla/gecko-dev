/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported MOCK_UNPOPULATED_DATA, MOCK_POPULATED_DATA, MOCK_INVALID_KEY_OBJ,
            MOCK_UNANALYZED_PRODUCT_RESPONSE, MOCK_STALE_PRODUCT_RESPONSE,
            MOCK_UNGRADED_PRODUCT_RESPONSE, MOCK_NOT_ENOUGH_REVIEWS_PRODUCT_RESPONSE,
            MOCK_ANALYZED_PRODUCT_RESPONSE, MOCK_UNAVAILABLE_PRODUCT_RESPONSE,
            MOCK_UNAVAILABLE_PRODUCT_REPORTED_RESPONSE, MOCK_PAGE_NOT_SUPPORTED_RESPONSE,
            MOCK_RECOMMENDED_ADS_RESPONSE, SUPPORTED_SITE_URL, verifyAnalysisDetailsVisible,
            verifyAnalysisDetailsHidden, verifyFooterHidden, getAnalysisDetails,
            getSettingsDetails, withReviewCheckerSidebar, reviewCheckerSidebarUpdated
            reviewCheckerSidebarAdsUpdated, verifyReviewCheckerSidebarProductInfo */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/shopping/test/browser/head.js",
  this
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const MOCK_UNPOPULATED_DATA = {
  adjusted_rating: null,
  grade: null,
  highlights: null,
};

const MOCK_POPULATED_DATA = {
  adjusted_rating: 5,
  grade: "B",
  highlights: {
    price: {
      positive: ["This watch is great and the price was even better."],
      negative: [],
      neutral: [],
    },
    quality: {
      positive: [
        "Other than that, I am very impressed with the watch and it’s capabilities.",
        "This watch performs above expectations in every way with the exception of the heart rate monitor.",
      ],
      negative: [
        "Battery life is no better than the 3 even with the solar gimmick, probably worse.",
      ],
      neutral: [
        "I have small wrists and still went with the 6X and glad I did.",
        "I can deal with the looks, as Im now retired.",
      ],
    },
    competitiveness: {
      positive: [
        "Bought this to replace my vivoactive 3.",
        "I like that this watch has so many features, especially those that monitor health like SP02, respiration, sleep, HRV status, stress, and heart rate.",
      ],
      negative: [
        "I do not use it for sleep or heartrate monitoring so not sure how accurate they are.",
      ],
      neutral: [
        "I've avoided getting a smartwatch for so long due to short battery life on most of them.",
      ],
    },
    "packaging/appearance": {
      positive: ["Great cardboard box."],
      negative: [],
      neutral: [],
    },
    shipping: {
      positive: [],
      negative: [],
      neutral: [],
    },
  },
};

const MOCK_INVALID_KEY_OBJ = {
  invalidHighlight: {
    negative: ["This is an invalid highlight and should not be visible"],
  },
  shipping: {
    positive: [],
    negative: [],
    neutral: [],
  },
};

const MOCK_UNANALYZED_PRODUCT_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  product_id: null,
  needs_analysis: true,
};

const MOCK_STALE_PRODUCT_RESPONSE = {
  ...MOCK_POPULATED_DATA,
  product_id: "ABCD123",
  grade: "A",
  needs_analysis: true,
};

const MOCK_UNGRADED_PRODUCT_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  product_id: "ABCD123",
  needs_analysis: true,
};

const MOCK_NOT_ENOUGH_REVIEWS_PRODUCT_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  product_id: "ABCD123",
  needs_analysis: false,
  not_enough_reviews: true,
};

const MOCK_ANALYZED_PRODUCT_RESPONSE = {
  ...MOCK_POPULATED_DATA,
  product_id: "ABCD123",
  needs_analysis: false,
};

const MOCK_UNAVAILABLE_PRODUCT_RESPONSE = {
  ...MOCK_POPULATED_DATA,
  product_id: "ABCD123",
  deleted_product: true,
};

const MOCK_UNAVAILABLE_PRODUCT_REPORTED_RESPONSE = {
  ...MOCK_UNAVAILABLE_PRODUCT_RESPONSE,
  deleted_product_reported: true,
};

const MOCK_PAGE_NOT_SUPPORTED_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  page_not_supported: true,
};

const MOCK_ERROR_RESPONSE = {
  ...MOCK_UNPOPULATED_DATA,
  error: "error",
};

const MOCK_RECOMMENDED_ADS_RESPONSE = [
  {
    name: "VIVO Electric 60 x 24 inch Stand Up Desk | Black Table Top, Black Frame, Height Adjustable Standing Workstation with Memory Preset Controller (DESK-KIT-1B6B)",
    url: "www.example.com",
    price: "249.99",
    currency: "USD",
    grade: "A",
    adjusted_rating: 4.6,
    sponsored: true,
    image_blob: new Blob(new Uint8Array(), { type: "image/jpeg" }),
  },
];

const SUPPORTED_SITE_URL = "https://example.com";

function verifyAnalysisDetailsVisible(shoppingContainer) {
  ok(
    shoppingContainer.reviewReliabilityEl,
    "review-reliability should be visible"
  );
  ok(shoppingContainer.adjustedRatingEl, "adjusted-rating should be visible");
  ok(shoppingContainer.highlightsEl, "review-highlights should be visible");
}

function verifyAnalysisDetailsHidden(shoppingContainer) {
  ok(
    !shoppingContainer.reviewReliabilityEl,
    "review-reliability should not be visible"
  );
  ok(
    !shoppingContainer.adjustedRatingEl,
    "adjusted-rating should not be visible"
  );
  ok(
    !shoppingContainer.highlightsEl,
    "review-highlights should not be visible"
  );
}

function verifyFooterVisible(shoppingContainer) {
  ok(shoppingContainer.settingsEl, "Got the shopping-settings element");
  ok(
    shoppingContainer.analysisExplainerEl,
    "Got the analysis-explainer element"
  );
}

function verifyFooterHidden(shoppingContainer) {
  ok(!shoppingContainer.settingsEl, "Do not render shopping-settings element");
  ok(
    !shoppingContainer.analysisExplainerEl,
    "Do not render the analysis-explainer element"
  );
}

function getAnalysisDetails(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.isProductPage = true;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;
    let returnState = {};
    for (let el of [
      "unanalyzedProductEl",
      "reviewReliabilityEl",
      "analysisExplainerEl",
      "adjustedRatingEl",
      "highlightsEl",
      "settingsEl",
      "shoppingMessageBarEl",
      "loadingEl",
    ]) {
      returnState[el] =
        !!shoppingContainer[el] &&
        ContentTaskUtils.isVisible(shoppingContainer[el]);
    }
    returnState.shoppingMessageBarType =
      shoppingContainer.shoppingMessageBarEl?.getAttribute("type");
    returnState.isOffline = shoppingContainer.isOffline;
    return returnState;
  });
}

function getSettingsDetails(browser, data) {
  return SpecialPowers.spawn(browser, [data], async mockData => {
    let shoppingContainer =
      content.document.querySelector("shopping-container").wrappedJSObject;
    shoppingContainer.data = Cu.cloneInto(mockData, content);
    await shoppingContainer.updateComplete;
    let shoppingSettings = shoppingContainer.settingsEl;
    await shoppingSettings.updateComplete;
    let returnState = {
      settingsEl:
        !!shoppingSettings && ContentTaskUtils.isVisible(shoppingSettings),
    };
    for (let el of ["recommendationsToggleEl", "optOutButtonEl"]) {
      returnState[el] =
        !!shoppingSettings[el] &&
        ContentTaskUtils.isVisible(shoppingSettings[el]);
    }
    return returnState;
  });
}

/**
 * Perform a task in the Review Checker sidebar's inner process,
 * after the sidebar browser and the RC browser have loaded.
 *
 * @param  {Function}  task      content task function
 * @param  {Array}     [args]    arguments for the content task
 * @param  {Window}    [win]     the window we expect the sidebar
 *                               to be loaded in
 */
async function withReviewCheckerSidebar(task, args = [], win = window) {
  const SHOPPING_SIDEBAR_URL = "about:shoppingsidebar";
  let sidebar = win.document.getElementById("sidebar");
  if (!sidebar) {
    return;
  }

  if (
    !win.SidebarController.isOpen ||
    win.SidebarController.currentID !== "viewReviewCheckerSidebar"
  ) {
    throw new Error("Review Checker is not shown");
  }

  let { readyState } = sidebar.contentDocument;
  if (readyState === "loading" || readyState === "uninitialized") {
    await new Promise(resolve => {
      sidebar.contentDocument.addEventListener("DOMContentLoaded", resolve, {
        once: true,
      });
    });
  }

  let rcBrowser = sidebar.contentDocument.getElementById(
    "review-checker-browser"
  );
  if (
    rcBrowser.webProgress.isLoadingDocument ||
    rcBrowser.currentURI?.spec !== SHOPPING_SIDEBAR_URL
  ) {
    await BrowserTestUtils.browserLoaded(
      rcBrowser,
      false,
      SHOPPING_SIDEBAR_URL
    );
  }

  await SpecialPowers.spawn(rcBrowser, args, task);
}

async function reviewCheckerSidebarUpdated(expectedProduct, win = window) {
  await withReviewCheckerSidebar(
    async prod => {
      let prodURI = Services.io.newURI(prod);
      function isLocationCurrent() {
        let actor = content.windowGlobalChild.getExistingActor("ReviewChecker");
        let currentURI = Services.io.newURI(actor?.currentURL);
        return currentURI.equalsExceptRef(prodURI);
      }
      function isUpdated() {
        let shoppingContainer =
          content.document.querySelector("shopping-container").wrappedJSObject;
        return (
          !!shoppingContainer.data || shoppingContainer.isProductPage === false
        );
      }

      if (isLocationCurrent() && isUpdated()) {
        info("Sidebar already loaded.");
        return true;
      }
      info(
        "Waiting for sidebar to be updated. Document: " +
          content.document.location.href
      );
      return ContentTaskUtils.waitForEvent(
        content.document,
        "Update",
        true,
        e => {
          info("Sidebar updated for product: " + JSON.stringify(e.detail));
          let hasData = !!e.detail.data;
          let isNotProductPage = e.detail.isProductPage === false;
          return (hasData || isNotProductPage) && isLocationCurrent();
        },
        true
      ).then(() => true);
    },
    [expectedProduct],
    win
  );
}

async function reviewCheckerSidebarAdsUpdated(expectedProduct, win = window) {
  await reviewCheckerSidebarUpdated(expectedProduct, win);
  await withReviewCheckerSidebar(
    () => {
      let container =
        content.document.querySelector("shopping-container").wrappedJSObject;
      if (container.recommendationData) {
        return true;
      }
      return ContentTaskUtils.waitForEvent(
        content.document,
        "UpdateRecommendations",
        true,
        null,
        true
      ).then(() => true);
    },
    [expectedProduct],
    win
  );
}

async function verifyReviewCheckerSidebarProductInfo(
  expectedProductInfo,
  win = window
) {
  await withReviewCheckerSidebar(
    async prodInfo => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      Assert.equal(
        shoppingContainer.reviewReliabilityEl.getAttribute("letter"),
        prodInfo.letterGrade,
        `Should have correct letter grade for product ${prodInfo.id}.`
      );
      Assert.equal(
        shoppingContainer.adjustedRatingEl.getAttribute("rating"),
        prodInfo.adjustedRating,
        `Should have correct adjusted rating for product ${prodInfo.id}.`
      );
      Assert.equal(
        content.windowGlobalChild.getExistingActor("ReviewChecker")?.currentURL,
        prodInfo.productURL,
        `Should have correct url in the child.`
      );
    },
    [expectedProductInfo],
    win
  );
}
