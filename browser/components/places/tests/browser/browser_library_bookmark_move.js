/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Tests whether the bookmarks in the library are
 * displayed in the correct order after moving.
 */

const GUIDS = [];
const PARENT_GUID = PlacesUtils.bookmarks.toolbarGuid;

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();

  for (let i = 0; i < 6; i++) {
    GUIDS.push(
      (
        await PlacesUtils.bookmarks.insert({
          parentGuid: PARENT_GUID,
          url: "https://example.com/" + i,
        })
      ).guid
    );
  }

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });
});

add_task(async function testMovingMultiple() {
  let libraryWin = await promiseLibrary("BookmarksToolbar");
  let tree = libraryWin.document.getElementById("placeContent");

  info("Moving first two bookmarks to the end.");
  await assertMoveAfter(tree, [0, 1], 5);
  info("Moving bookmark 5 to index 1.");
  await assertMoveAfter(tree, [5], 0);

  await promiseLibraryClosed(libraryWin);
});

/**
 * Moves an array of bookmarks after the specified index in the library and
 * asserts that the order in the tree is consistent with the order in the library.
 *
 * @param {XULTreeElement} tree
 *   The tree element.
 * @param {number[]} nodes
 *   An array containing the bookmarks to be moved (their indices in `GUIDS`).
 * @param {number} anchor
 *   The index of the row in the tree the bookmarks should be moved after.
 */
async function assertMoveAfter(tree, nodes, anchor) {
  let guids = nodes.map(n => GUIDS[n]);

  tree.selectItems(guids);
  let data = tree.selectedNodes.map(n =>
    PlacesUtils.wrapNode(n, "text/x-moz-place")
  );

  let dataTransfer = {
    dropEffect: "move",
    mozCursor: "auto",
    mozItemCount: nodes.length,
    mozTypesAt() {
      return ["text/x-moz-place"];
    },
    mozGetDataAt(_, i) {
      return data[i];
    },
    mozSetDataAt(_type, newData, _index) {
      data.push(newData);
    },
  };

  let dragStartEvent = new CustomEvent("dragstart", {
    bubbles: true,
  });
  dragStartEvent.dataTransfer = dataTransfer;

  let dragEndEvent = new CustomEvent("dragend", {
    bubbles: true,
  });
  dragEndEvent.dataTransfer = dataTransfer;

  tree.treeBody.dispatchEvent(dragStartEvent);
  await tree.view.drop(anchor, Ci.nsITreeView.DROP_AFTER, dataTransfer);
  tree.treeBody.dispatchEvent(dragEndEvent);

  await assertOrder(tree);
}

/**
 * Asserts that the order in the tree is consistent with the order in the library.
 *
 * @param {XULTreeElement} tree
 *   The tree element.
 */
async function assertOrder(tree) {
  let bookmarks = (await PlacesUtils.promiseBookmarksTree(PARENT_GUID))
    .children;

  for (let i = 0; i < bookmarks.length; i++) {
    Assert.equal(
      tree.view._getNodeForRow(i).bookmarkGuid,
      bookmarks[i].guid,
      `Row ${i} is as expected.`
    );
  }
}
