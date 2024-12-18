/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

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

      // Simulate reload by calling init on the delete card
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

add_task(async function test_bookmark_counts() {
  await initGroupDatabase();

  await lazy.PlacesUtils.bookmarks.eraseEverything();

  await lazy.PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "Example",
    url: "https://example.com",
  });
  await lazy.PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "Example 2",
    url: "https://example.net",
  });
  await lazy.PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "Example 3",
    url: "https://example.org",
  });

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

        let bookmarkCounts =
          deleteProfileCard.shadowRoot.querySelector(
            "#bookmarks b"
          ).textContent;

        Assert.equal(
          3,
          bookmarkCounts,
          "Should display expected bookmarks count"
        );
      });
    }
  );

  await lazy.PlacesUtils.bookmarks.eraseEverything();
});
