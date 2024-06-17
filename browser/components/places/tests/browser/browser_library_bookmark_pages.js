/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the a new bookmark is correctly selected after being created via
 * the bookmark dialog.
 */
"use strict";

const TEST_URIS = ["https://example1.com/", "https://example2.com/"];
let library;

add_setup(async function () {
  await PlacesTestUtils.addVisits(TEST_URIS);

  library = await promiseLibrary("History");

  registerCleanupFunction(async function () {
    await promiseLibraryClosed(library);
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_bookmark_page() {
  library.ContentTree.view.selectPlaceURI(TEST_URIS[0]);

  await withBookmarksDialog(
    true,
    async () => {
      // Open the context menu.
      let placesContext = library.document.getElementById("placesContext");
      let promisePopup = BrowserTestUtils.waitForEvent(
        placesContext,
        "popupshown"
      );
      synthesizeClickOnSelectedTreeCell(library.ContentTree.view, {
        button: 2,
        type: "contextmenu",
      });

      await promisePopup;
      let properties = library.document.getElementById(
        "placesContext_createBookmark"
      );
      placesContext.activateItem(properties);
    },
    async dialogWin => {
      Assert.strictEqual(
        dialogWin.BookmarkPropertiesPanel._itemType,
        0,
        "Should have loaded a bookmark dialog"
      );
      Assert.equal(
        dialogWin.document.getElementById("editBMPanel_locationField").value,
        TEST_URIS[0],
        "Should have opened the dialog with the correct uri to be bookmarked"
      );
    }
  );
});

add_task(async function test_bookmark_pages() {
  library.ContentTree.view.selectAll();

  await withBookmarksDialog(
    false,
    async () => {
      // Open the context menu.
      let placesContext = library.document.getElementById("placesContext");
      let promisePopup = BrowserTestUtils.waitForEvent(
        placesContext,
        "popupshown"
      );
      synthesizeClickOnSelectedTreeCell(library.ContentTree.view, {
        button: 2,
        type: "contextmenu",
      });

      await promisePopup;
      let properties = library.document.getElementById(
        "placesContext_createBookmark"
      );
      placesContext.activateItem(properties);
    },
    async dialogWin => {
      Assert.strictEqual(
        dialogWin.BookmarkPropertiesPanel._itemType,
        1,
        "Should have loaded a create bookmark folder dialog"
      );
      Assert.deepEqual(
        dialogWin.BookmarkPropertiesPanel._URIs.map(uri => uri.uri.spec),
        // The list here is reversed, because that's the order they're shown
        // in the view.
        [TEST_URIS[1], TEST_URIS[0]],
        "Should have got the correct URIs for adding to the folder"
      );

      let promiseBookmarkAdded =
        PlacesTestUtils.waitForNotification("bookmark-added");
      let promiseTagsChanged = PlacesTestUtils.waitForNotification(
        "bookmark-tags-changed"
      );

      fillBookmarkTextField("editBMPanel_namePicker", "folder", dialogWin);
      fillBookmarkTextField(
        "editBMPanel_tagsField",
        "tag1,tag2,tag3",
        dialogWin
      );

      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);

      await Promise.all([promiseBookmarkAdded, promiseTagsChanged]);

      for (const url of TEST_URIS) {
        const { parentGuid } = await PlacesUtils.bookmarks.fetch({ url });
        const folder = await PlacesUtils.bookmarks.fetch({
          guid: parentGuid,
        });
        Assert.equal(
          folder.title,
          "folder",
          "Should have created the bookmark in the right folder."
        );

        const tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(url));
        Assert.deepEqual(
          tags,
          ["tag1", "tag2", "tag3"],
          "Found the expected tags"
        );
      }
    }
  );

  // Reset tags.
  for (const url of TEST_URIS) {
    PlacesUtils.tagging.untagURI(Services.io.newURI(url), null);
  }
});

add_task(async function test_bookmark_pages_with_existing_tags() {
  library.ContentTree.view.selectAll();

  PlacesUtils.tagging.tagURI(Services.io.newURI(TEST_URIS[0]), [
    "common_1",
    "common_2",
    "tag_1",
  ]);
  PlacesUtils.tagging.tagURI(Services.io.newURI(TEST_URIS[1]), [
    "common_1",
    "common_2",
    "tag_2",
  ]);

  await withBookmarksDialog(
    false,
    async () => {
      // Open the context menu.
      let placesContext = library.document.getElementById("placesContext");
      let promisePopup = BrowserTestUtils.waitForEvent(
        placesContext,
        "popupshown"
      );
      synthesizeClickOnSelectedTreeCell(library.ContentTree.view, {
        button: 2,
        type: "contextmenu",
      });

      await promisePopup;
      let properties = library.document.getElementById(
        "placesContext_createBookmark"
      );
      placesContext.activateItem(properties);
    },
    async dialogWin => {
      Assert.equal(
        dialogWin.document.getElementById("editBMPanel_tagsField").value,
        "common_1, common_2",
        "Shown the common tags"
      );

      let promiseBookmarkAdded =
        PlacesTestUtils.waitForNotification("bookmark-added");
      let promiseTagsChanged = PlacesTestUtils.waitForNotification(
        "bookmark-tags-changed"
      );

      fillBookmarkTextField("editBMPanel_namePicker", "folder", dialogWin);
      fillBookmarkTextField(
        "editBMPanel_tagsField",
        "common_2, common_3",
        dialogWin
      );

      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);

      await Promise.all([promiseBookmarkAdded, promiseTagsChanged]);

      for (const { url, expectedTags } of [
        { url: TEST_URIS[0], expectedTags: ["common_2", "common_3", "tag_1"] },
        { url: TEST_URIS[1], expectedTags: ["common_2", "common_3", "tag_2"] },
      ]) {
        const { parentGuid } = await PlacesUtils.bookmarks.fetch({ url });
        const folder = await PlacesUtils.bookmarks.fetch({
          guid: parentGuid,
        });
        Assert.equal(
          folder.title,
          "folder",
          "Should have created the bookmark in the right folder."
        );

        const tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(url));
        Assert.deepEqual(tags, expectedTags, "Found the expected tags");
      }
    }
  );

  // Reset tags.
  for (const url of TEST_URIS) {
    PlacesUtils.tagging.untagURI(Services.io.newURI(url), null);
  }
});
