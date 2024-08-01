/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_selectingElementsInShadowRoots() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHADOWROOT_TEST_PAGE,
    },
    async browser => {
      let elementDimensions = await SpecialPowers.spawn(
        browser,
        [],
        async () => {
          let firstTestPageDiv = content.document
            .querySelector("div")
            .openOrClosedShadowRoot.querySelector("#testPageElement");

          let secondTestPageDiv = firstTestPageDiv.openOrClosedShadowRoot
            .querySelector("div")
            .openOrClosedShadowRoot.querySelector("#nestedTestPageElement");

          return [
            firstTestPageDiv.getBoundingClientRect(),
            secondTestPageDiv.getBoundingClientRect(),
          ];
        }
      );

      info(JSON.stringify(elementDimensions, null, 2));

      let helper = new ScreenshotsHelper(browser);
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      for (let el of elementDimensions) {
        let x = el.left + el.width / 2;
        let y = el.top + el.height / 2;

        mouse.move(x, y);
        await helper.waitForHoverElementRect(el.width, el.height);
        mouse.click(x, y);

        await helper.waitForStateChange(["selected"]);

        let dimensions = await helper.getSelectionRegionDimensions();

        is(
          dimensions.left,
          el.left,
          "The region left position matches the elements left position"
        );
        is(
          dimensions.top,
          el.top,
          "The region top position matches the elements top position"
        );
        is(
          dimensions.width,
          el.width,
          "The region width matches the elements width"
        );
        is(
          dimensions.height,
          el.height,
          "The region height matches the elements height"
        );

        mouse.click(500, 500);
        await helper.waitForStateChange(["crosshairs"]);
      }
    }
  );
});
