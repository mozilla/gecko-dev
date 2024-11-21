/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_serviceInitialized() {
  await initGroupDatabase();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:deleteprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let deleteProfileCard = content.document.querySelector(
          "delete-profile-card"
        ).wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => deleteProfileCard.initialized,
          "Waiting for delete-profile-card to be initialized"
        );

        Assert.ok(
          ContentTaskUtils.isVisible(deleteProfileCard),
          "The delete-profile-card is visible"
        );
      });

      await SelectableProfileService.uninit();
      Assert.ok(
        !SelectableProfileService.initialized,
        "The SelectableProfileService is uninitialized"
      );

      // Simiulate reload by calling init on the delete card
      await SpecialPowers.spawn(browser, [], async () => {
        let deleteProfileCard = content.document.querySelector(
          "delete-profile-card"
        ).wrappedJSObject;

        deleteProfileCard.initialized = false;
        await ContentTaskUtils.waitForCondition(
          () => !deleteProfileCard.initialized,
          "Waiting for delete-profile-card to be uninitialized"
        );

        deleteProfileCard.init();

        await ContentTaskUtils.waitForCondition(
          () => deleteProfileCard.initialized,
          "Waiting for delete-profile-card to be initialized"
        );

        Assert.ok(
          ContentTaskUtils.isVisible(deleteProfileCard),
          "The delete-profile-card is visible"
        );
      });
    }
  );
});
