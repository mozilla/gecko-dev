/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue is correctly interpreting the preferences settable
 * by the user or by other components.
 */

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_AUTO_EXPORT_HTML = "browser.bookmarks.autoExportHTML";
const PREF_IMPORT_BOOKMARKS_HTML = "browser.places.importBookmarksHTML";
const PREF_RESTORE_DEFAULT_BOOKMARKS = "browser.bookmarks.restore_default_bookmarks";

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";

/**
 * Rebuilds smart bookmarks listening to console output to report any message or
 * exception generated when calling ensurePlacesDefaultQueriesInitialized().
 */
function rebuildSmartBookmarks() {
  let consoleListener = {
    observe: function(aMsg) {
      print("Got console message: " + aMsg.message);
    },

    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIConsoleListener
    ]),
  };
  Services.console.reset();
  Services.console.registerListener(consoleListener);
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIBrowserGlue)
                                          .ensurePlacesDefaultQueriesInitialized();
  Services.console.unregisterListener(consoleListener);
}


let tests = [];
//------------------------------------------------------------------------------

tests.push({
  description: "All smart bookmarks are created if smart bookmarks version is 0.",
  exec: function() {
    // Sanity check: we should have default bookmark.
    do_check_neq(PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0), -1);
    do_check_neq(PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 0), -1);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

    rebuildSmartBookmarks();

    // Count items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check version has been updated.
    do_check_eq(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "An existing smart bookmark is replaced when version changes.",
  exec: function() {
    // Sanity check: we have a smart bookmark on the toolbar.
    let itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_neq(itemId, -1);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));
    // Change its title.
    PlacesUtils.bookmarks.setItemTitle(itemId, "new title");
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), "new title");

    // Sanity check items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

    rebuildSmartBookmarks();

    // Count items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check smart bookmark has been replaced, itemId has changed.
    itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_neq(itemId, -1);
    do_check_neq(PlacesUtils.bookmarks.getItemTitle(itemId), "new title");
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));

    // Check version has been updated.
    do_check_eq(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "bookmarks position is retained when version changes.",
  exec: function() {
    // Sanity check items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    let itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 0);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));
    let firstItemTitle = PlacesUtils.bookmarks.getItemTitle(itemId);

    itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 1);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));
    let secondItemTitle = PlacesUtils.bookmarks.getItemTitle(itemId);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

    rebuildSmartBookmarks();

    // Count items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check smart bookmarks are still in correct position.
    itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 0);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), firstItemTitle);

    itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 1);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), secondItemTitle);

    // Check version has been updated.
    do_check_eq(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "moved bookmarks position is retained when version changes.",
  exec: function() {
    // Sanity check items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    let itemId1 = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 0);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId1, SMART_BOOKMARKS_ANNO));
    let firstItemTitle = PlacesUtils.bookmarks.getItemTitle(itemId1);

    let itemId2 = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 1);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId2, SMART_BOOKMARKS_ANNO));
    let secondItemTitle = PlacesUtils.bookmarks.getItemTitle(itemId2);

    // Move the first smart bookmark to the end of the menu.
    PlacesUtils.bookmarks.moveItem(itemId1, PlacesUtils.bookmarksMenuFolderId,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);

    do_check_eq(itemId1, PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId,
                                                   PlacesUtils.bookmarks.DEFAULT_INDEX));

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

    rebuildSmartBookmarks();

    // Count items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check smart bookmarks are still in correct position.
    itemId2 = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 0);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId2, SMART_BOOKMARKS_ANNO));
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId2), secondItemTitle);

    itemId1 = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId,
                                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_check_true(PlacesUtils.annotations.itemHasAnnotation(itemId1, SMART_BOOKMARKS_ANNO));
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId1), firstItemTitle);

    // Move back the smart bookmark to the original position.
    PlacesUtils.bookmarks.moveItem(itemId1, PlacesUtils.bookmarksMenuFolderId, 1);

    // Check version has been updated.
    do_check_eq(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "An explicitly removed smart bookmark should not be recreated.",
  exec: function() {   
    // Remove toolbar's smart bookmarks
    PlacesUtils.bookmarks.removeItem(PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0));

    // Sanity check items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

    rebuildSmartBookmarks();

    // Count items.
    // We should not have recreated the smart bookmark on toolbar.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check version has been updated.
    do_check_eq(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "Even if a smart bookmark has been removed recreate it if version is 0.",
  exec: function() {
    // Sanity check items.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

    rebuildSmartBookmarks();

    // Count items.
    // We should not have recreated the smart bookmark on toolbar.
    do_check_eq(countFolderChildren(PlacesUtils.toolbarFolderId),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check version has been updated.
    do_check_eq(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});
//------------------------------------------------------------------------------

function countFolderChildren(aFolderItemId) {
  let rootNode = PlacesUtils.getFolderContents(aFolderItemId).root;
  let cc = rootNode.childCount;
  // Dump contents.
  for (let i = 0; i < cc ; i++) {
    let node = rootNode.getChild(i);
    let title = PlacesUtils.nodeIsSeparator(node) ? "---" : node.title;
    print("Found child(" + i + "): " + title);
  }
  rootNode.containerOpen = false;
  return cc;
}

function next_test() {
  if (tests.length) {
    // Execute next test.
    let test = tests.shift();
    print("\nTEST: " + test.description);
    test.exec();
  }
  else {
    // Clean up database from all bookmarks.
    remove_all_bookmarks();
    do_test_finished();
  }
}

function run_test() {
  do_test_pending();

  remove_bookmarks_html();
  remove_all_JSON_backups();

  // Initialize browserGlue, but remove it's listener to places-init-complete.
  let bg = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver);
  // Initialize Places.
  PlacesUtils.history;
  // Observes Places initialisation complete.
  Services.obs.addObserver(function waitPlaceInitComplete() {
    Services.obs.removeObserver(waitPlaceInitComplete, "places-browser-init-complete");

    // Ensure preferences status.
    do_check_false(Services.prefs.getBoolPref(PREF_AUTO_EXPORT_HTML));
    do_check_false(Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
    try {
      do_check_false(Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
      do_throw("importBookmarksHTML pref should not exist");
    }
    catch(ex) {}

    waitForImportAndSmartBookmarks(next_test);
  }, "places-browser-init-complete", false);

  // Usually places init would async notify to glue, but we want to avoid
  // randomness here, thus we fire the notification synchronously.
  bg.observe(null, "places-init-complete", null);
}

function waitForImportAndSmartBookmarks(aCallback) {
  Services.obs.addObserver(function waitImport() {
    Services.obs.removeObserver(waitImport, "bookmarks-restore-success");
    // Delay to test eventual smart bookmarks creation.
    do_execute_soon(function () {
      promiseAsyncUpdates().then(aCallback);
    });
  }, "bookmarks-restore-success", false);
}
