package org.mozilla.gecko.tests;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.StringUtils;

public class testBookmarksPanel extends AboutHomeTest {
    public void testBookmarksPanel() {
        final String BOOKMARK_URL = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        JSONObject data = null;

        // Make sure our default bookmarks are loaded.
        // Technically this will race with the check below.
        initializeProfile();

        // Add a mobile bookmark.
        mDatabaseHelper.addOrUpdateMobileBookmark(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE, BOOKMARK_URL);

        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);

        // Check that the default bookmarks are displayed.
        // We need to wait for the distribution to have been processed
        // before this will succeed.
        for (String url : StringHelper.DEFAULT_BOOKMARKS_URLS) {
            isBookmarkDisplayed(url);
        }

        assertAllContextMenuOptionsArePresent(StringHelper.DEFAULT_BOOKMARKS_URLS[1],
                StringHelper.DEFAULT_BOOKMARKS_URLS[0]);

        openBookmarkContextMenu(StringHelper.DEFAULT_BOOKMARKS_URLS[0]);

        // Test that "Open in New Tab" works
        final Element tabCount = mDriver.findElement(getActivity(), R.id.tabs_counter);
        final int tabCountInt = Integer.parseInt(tabCount.getText());
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        mSolo.clickOnText(StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS[0]);
        try {
            data = new JSONObject(tabEventExpecter.blockForEventData());
        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting event data", e.toString());
        }
        tabEventExpecter.unregisterListener();
        mAsserter.ok(mSolo.searchText(StringHelper.TITLE_PLACE_HOLDER), "Checking that the tab is not changed", "The tab was not changed");
        // extra check here on the Tab:Added message to be sure the right tab opened
        int tabID = 0;
        try {
            mAsserter.is(StringHelper.ABOUT_FIREFOX_URL, data.getString("uri"), "Checking tab uri");
            tabID = data.getInt("tabID");
        } catch (JSONException e) {
            mAsserter.ok(false, "exception accessing event data", e.toString());
        }
        // close tab so about:firefox can be selected again
        closeTab(tabID);

        // Test that "Open in Private Tab" works
        openBookmarkContextMenu(StringHelper.DEFAULT_BOOKMARKS_URLS[0]);
        tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        mSolo.clickOnText(StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS[1]);
        try {
            data = new JSONObject(tabEventExpecter.blockForEventData());
        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting event data", e.toString());
        }
        tabEventExpecter.unregisterListener();
        mAsserter.ok(mSolo.searchText(StringHelper.TITLE_PLACE_HOLDER), "Checking that the tab is not changed", "The tab was not changed");
        // extra check here on the Tab:Added message to be sure the right tab opened, again
        try {
            mAsserter.is(StringHelper.ABOUT_FIREFOX_URL, data.getString("uri"), "Checking tab uri");
        } catch (JSONException e) {
            mAsserter.ok(false, "exception accessing event data", e.toString());
        }

        // Test that "Edit" works
        String[] editedBookmarkValues = new String[] { "New bookmark title", "www.NewBookmark.url", "newBookmarkKeyword" };
        editBookmark(BOOKMARK_URL, editedBookmarkValues);
        checkBookmarkEdit(editedBookmarkValues[1], editedBookmarkValues);

        // Test that "Remove" works
        openBookmarkContextMenu(editedBookmarkValues[1]);
        mSolo.clickOnText(StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS[3]);
        waitForText("Bookmark removed");
        mAsserter.ok(!mDatabaseHelper.isBookmark(editedBookmarkValues[1]), "Checking that the bookmark was removed", "The bookmark was removed");
    }

    /**
     * Asserts that all context menu items are present on the given links. For one link,
     * the context menu is expected to not have the "Share" context menu item.
     *
     * @param shareableURL A URL that is expected to have the "Share" context menu item
     * @param nonShareableURL A URL that is expected not to have the "Share" context menu item.
     */
    private void assertAllContextMenuOptionsArePresent(final String shareableURL,
            final String nonShareableURL) {
        mAsserter.ok(StringUtils.isShareableUrl(shareableURL), "Ensuring url is shareable", "");
        mAsserter.ok(!StringUtils.isShareableUrl(nonShareableURL), "Ensuring url is not shareable", "");

        openBookmarkContextMenu(shareableURL);
        for (String contextMenuOption : StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS) {
            mAsserter.ok(mSolo.searchText(contextMenuOption),
                    "Checking that the context menu option is present",
                    contextMenuOption + " is present");
        }

        // Close the menu.
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);

        openBookmarkContextMenu(nonShareableURL);
        for (String contextMenuOption : StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS) {
            // This link is not shareable: skip the "Share" option.
            if ("Share".equals(contextMenuOption)) {
                continue;
            }

            mAsserter.ok(mSolo.searchText(contextMenuOption),
                    "Checking that the context menu option is present",
                    contextMenuOption + " is present");
        }

        // Close the menu.
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
    }

   /**
    * @param bookmarkUrl URL of the bookmark to edit
    * @param values String array with the new values for all fields
    */
    private void editBookmark(String bookmarkUrl, String[] values) {
        openBookmarkContextMenu(bookmarkUrl);
        mSolo.clickOnText("Edit");
        waitForText("Edit Bookmark");

        // Update the fields with the new values
        for (int i = 0; i < values.length; i++) {
            mSolo.clearEditText(i);
            mSolo.clickOnEditText(i);
            mActions.sendKeys(values[i]);
        }

        mSolo.clickOnButton("OK");
        waitForText("Bookmark updated");
    }

   /**
    * @param bookmarkUrl String with the original url
    * @param values String array with the new values for all fields
    */
    private void checkBookmarkEdit(String bookmarkUrl, String[] values) {
        openBookmarkContextMenu(bookmarkUrl);
        mSolo.clickOnText("Edit");
        waitForText("Edit Bookmark");

        // Check the values of the fields
        for (String value : values) {
            mAsserter.ok(mSolo.searchText(value), "Checking that the value is correct", "The value = " + value + " is correct");
        }

        mSolo.clickOnButton("Cancel");
        waitForText("BOOKMARKS");
    }
}
