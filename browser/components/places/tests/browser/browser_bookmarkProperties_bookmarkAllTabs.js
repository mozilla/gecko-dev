"use strict";

const TEST_URLS = ["about:robots", "about:mozilla"];

add_task(async function bookmark_all_tabs() {
  let tabs = [];
  for (let url of TEST_URLS) {
    tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, url));
  }
  registerCleanupFunction(async function () {
    for (let tab of tabs) {
      BrowserTestUtils.removeTab(tab);
    }

    for (const url of TEST_URLS) {
      PlacesUtils.tagging.untagURI(Services.io.newURI(url), null);
    }

    await PlacesUtils.bookmarks.eraseEverything();
  });

  await withBookmarksDialog(
    false,
    function open() {
      document.getElementById("Browser:BookmarkAllTabs").doCommand();
    },
    async dialog => {
      let acceptBtn = dialog.document
        .getElementById("bookmarkpropertiesdialog")
        .getButton("accept");
      Assert.ok(!acceptBtn.disabled, "Accept button is enabled");

      let namepicker = dialog.document.getElementById("editBMPanel_namePicker");
      Assert.ok(!namepicker.readOnly, "Name field is writable");
      let folderName = dialog.document
        .getElementById("stringBundle")
        .getString("bookmarkAllTabsDefault");
      Assert.equal(namepicker.value, folderName, "Name field is correct.");

      let promiseBookmarkAdded =
        PlacesTestUtils.waitForNotification("bookmark-added");
      let promiseTagsChanged = PlacesTestUtils.waitForNotification(
        "bookmark-tags-changed"
      );

      fillBookmarkTextField("editBMPanel_namePicker", "folder", dialog);
      fillBookmarkTextField("editBMPanel_tagsField", "tag1,tag2,tag3", dialog);

      let folderPicker = dialog.document.getElementById(
        "editBMPanel_folderMenuList"
      );

      let defaultParentGuid = await PlacesUIUtils.defaultParentGuid;
      // Check the initial state of the folder picker.
      await TestUtils.waitForCondition(
        () => folderPicker.getAttribute("selectedGuid") == defaultParentGuid,
        "The folder is the expected one."
      );

      EventUtils.synthesizeKey("VK_RETURN", {}, dialog);

      await Promise.all([promiseBookmarkAdded, promiseTagsChanged]);
      for (const url of TEST_URLS) {
        const { parentGuid } = await PlacesUtils.bookmarks.fetch({ url });
        const folder = await PlacesUtils.bookmarks.fetch({
          guid: parentGuid,
        });
        is(
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
});
