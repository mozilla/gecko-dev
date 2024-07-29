/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_screenshotsFace() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await SpecialPowers.spawn(browser, [], async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        content.focus();
        screenshotsChild.overlay.previewFace.focus({ focusVisible: true });
      });

      key.down(" ");

      await helper.waitForStateChange(["selected"]);

      await SpecialPowers.spawn(browser, [], () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        is(
          Services.focus.focusedElement,
          screenshotsChild.overlay.bottomRightMover,
          "The bottom right mover should be focused"
        );
      });

      helper.triggerUIFromToolbar();
      await helper.waitForOverlayClosed();

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await SpecialPowers.spawn(browser, [], async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        content.focus();
        screenshotsChild.overlay.previewFace.focus({ focusVisible: true });
      });

      key.down("Enter");

      await helper.waitForStateChange(["selected"]);

      await SpecialPowers.spawn(browser, [], () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );

        is(
          Services.focus.focusedElement,
          screenshotsChild.overlay.bottomRightMover,
          "The bottom right mover should be focused"
        );
      });
    }
  );
});
