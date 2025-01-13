/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_adjusted_rating() {
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [MOCK_ANALYZED_PRODUCT_RESPONSE],
        async mockData => {
          let rating = mockData.adjusted_rating;

          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;
          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let adjustedRating = shoppingContainer.adjustedRatingEl;
          await adjustedRating.updateComplete;

          is(
            adjustedRating.rating,
            rating,
            `The adjusted rating "rating" is ${rating}`
          );

          rating = 2.55;
          adjustedRating.rating = rating;

          await adjustedRating.updateComplete;

          is(
            adjustedRating.rating,
            rating,
            `The adjusted rating "rating" is now ${rating}`
          );

          rating = 0;
          adjustedRating.rating = rating;

          await adjustedRating.updateComplete;

          is(
            adjustedRating.rating,
            rating,
            `The adjusted rating "rating" is now ${rating}`
          );

          rating = null;
          adjustedRating.rating = rating;

          await adjustedRating.updateComplete;

          is(
            adjustedRating.rating,
            rating,
            `The adjusted rating "rating" is now ${rating}`
          );

          ok(
            ContentTaskUtils.isHidden(adjustedRating),
            "adjusted rating should not be visible"
          );

          rating = 3;
          adjustedRating.rating = rating;

          await adjustedRating.updateComplete;
          ok(
            ContentTaskUtils.isVisible(adjustedRating),
            "adjusted rating should be visible"
          );
          is(
            adjustedRating.rating,
            rating,
            `The adjusted rating "rating" is now ${rating}`
          );
        }
      );
    }
  );
});
