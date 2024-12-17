/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import android.util.Log
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsNotDisplayed
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onChildAt
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextClearance
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.performTouchInput
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.clearText
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.res
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.containsString
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.snackbarButton
import org.mozilla.fenix.helpers.TestHelper.waitUntilSnackbarGone
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.library.bookmarks.BookmarksTestTag.addBookmarkFolderNameTextField
import org.mozilla.fenix.library.bookmarks.BookmarksTestTag.editBookmarkedItemTileTextField
import org.mozilla.fenix.library.bookmarks.BookmarksTestTag.editBookmarkedItemURLTextField

/**
 * Implementation of Robot Pattern for the bookmarks menu.
 */
class BookmarksRobot {

    fun verifyBookmarksMenuView() {
        Log.i(TAG, "verifyBookmarksMenuView: Waiting for $waitingTimeShort ms for bookmarks view to exist")
        mDevice.findObject(
            UiSelector().text("Bookmarks"),
        ).waitForExists(waitingTimeShort)
        Log.i(TAG, "verifyBookmarksMenuView: Waited for $waitingTimeShort ms for bookmarks view to exist")
        Log.i(TAG, "verifyBookmarksMenuView: Trying to verify bookmarks view is displayed")
        onView(
            allOf(
                withText("Bookmarks"),
                withParent(withId(R.id.navigationToolbar)),
            ),
        ).check(matches(isDisplayed()))
        Log.i(TAG, "verifyBookmarksMenuView: Verified bookmarks view is displayed")
    }

    fun verifyAddFolderButton() {
        Log.i(TAG, "verifyAddFolderButton: Trying to verify add bookmarks folder button is visible")
        addFolderButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyAddFolderButton: Verified add bookmarks folder button is visible")
    }

    fun verifyCloseButton() {
        Log.i(TAG, "verifyCloseButton: Trying to verify close bookmarks section button is visible")
        closeButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseButton: Verified close bookmarks section button is visible")
    }

    fun verifyBookmarkFavicon(forUrl: Uri) {
        Log.i(TAG, "verifyBookmarkFavicon: Trying to verify bookmarks favicon for $forUrl is visible")
        bookmarkFavicon(forUrl.toString()).check(
            matches(
                withEffectiveVisibility(
                    ViewMatchers.Visibility.VISIBLE,
                ),
            ),
        )
        Log.i(TAG, "verifyBookmarkFavicon: Verified bookmarks favicon for $forUrl is visible")
    }

    fun verifyBookmarkedURL(url: String) {
        Log.i(TAG, "verifyBookmarkedURL: Trying to verify bookmarks url: $url is displayed")
        bookmarkURL(url).check(matches(isDisplayed()))
        Log.i(TAG, "verifyBookmarkedURL: Verified bookmarks url: $url is displayed")
    }

    fun verifyRedesignedBookmarkURL(composeTestRule: ComposeTestRule, url: String) {
        Log.i(TAG, "verifyRedesignedBookmarkURL: Trying to verify bookmarks url: $url is displayed")
        composeTestRule.onNodeWithText(url).assertIsDisplayed()
        Log.i(TAG, "verifyRedesignedBookmarkURL: Verified bookmarks url: $url is displayed")
    }

    fun verifyFolderTitle(title: String) {
        Log.i(TAG, "verifyFolderTitle: Waiting for $waitingTime ms for bookmarks folder with title: $title to exist")
        mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTime)
        Log.i(TAG, "verifyFolderTitle: Waited for $waitingTime ms for bookmarks folder with title: $title to exist")
        Log.i(TAG, "verifyFolderTitle: Trying to verify bookmarks folder with title: $title is displayed")
        onView(withText(title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyFolderTitle: Verified bookmarks folder with title: $title is displayed")
    }

    fun verifyFolderTitleFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, title: String) {
        Log.i(TAG, "verifyFolderTitleFromRedesignedBookmarksMenu: Trying to verify bookmarks folder with title: $title is displayed")
        composeTestRule.onNodeWithText(title).assertIsDisplayed()
        Log.i(TAG, "verifyFolderTitleFromRedesignedBookmarksMenu: Verified bookmarks folder with title: $title is displayed")
    }

    fun verifyBookmarkFolderIsNotCreated(title: String) {
        Log.i(TAG, "verifyBookmarkFolderIsNotCreated: Waiting for $waitingTime ms for bookmarks folder with title: $title to exist")
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/bookmarks_wrapper"),
        ).waitForExists(waitingTime)
        Log.i(TAG, "verifyBookmarkFolderIsNotCreated: Waited for $waitingTime ms for bookmarks folder with title: $title to exist")

        assertUIObjectExists(itemContainingText(title), exists = false)
    }

    fun verifyBookmarkTitle(title: String) {
        Log.i(TAG, "verifyBookmarkTitle: Waiting for $waitingTime ms for bookmark with title: $title to exist")
        mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTime)
        Log.i(TAG, "verifyBookmarkTitle: Waited for $waitingTime ms for bookmark with title: $title to exist")
        Log.i(TAG, "verifyBookmarkTitle: Trying to verify bookmark with title: $title is displayed")
        onView(withText(title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyBookmarkTitle: Verified bookmark with title: $title is displayed")
    }

    fun verifyRedesignedBookmarkTitle(composeTestRule: ComposeTestRule, title: String) {
        Log.i(TAG, "verifyRedesignedBookmarkTitle: Trying to verify bookmark with title: $title is displayed")
        composeTestRule.onNodeWithText(title).assertIsDisplayed()
        Log.i(TAG, "verifyRedesignedBookmarkTitle: Verified bookmark with title: $title is displayed")
    }

    fun verifyBookmarkIsDeleted(expectedTitle: String) {
        Log.i(TAG, "verifyBookmarkIsDeleted: Waiting for $waitingTime ms for bookmarks view to exist")
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/bookmarks_wrapper"),
        ).waitForExists(waitingTime)
        Log.i(TAG, "verifyBookmarkIsDeleted: Waited for $waitingTime ms for bookmarks view to exist")
        assertUIObjectExists(
            itemWithResIdContainingText(
                "$packageName:id/title",
                expectedTitle,
            ),
            exists = false,
        )
    }

    fun verifyBookmarkIsDeletedFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, expectedTitle: String) {
        Log.i(TAG, "verifyBookmarkIsDeletedFromRedesignedBookmarksMenu: Trying to verify that the bookmarked item : $expectedTitle is not displayed")
        composeTestRule.onNodeWithText(expectedTitle).assertIsNotDisplayed()
        Log.i(TAG, "verifyBookmarkIsDeletedFromRedesignedBookmarksMenu: Verified that the bookmarked item : $expectedTitle is not displayed")
    }

    fun verifyUndoDeleteSnackBarButton() {
        Log.i(TAG, "verifyUndoDeleteSnackBarButton: Trying to verify bookmark deletion undo snack bar button")
        assertTrue(snackbarButton!!.children.count { it.text == "UNDO" } == 1)
        Log.i(TAG, "verifyUndoDeleteSnackBarButton: Verified bookmark deletion undo snack bar button")
    }

    fun verifySnackBarHidden() {
        Log.i(TAG, "verifySnackBarHidden: Waiting until undo snack bar button is gone")
        mDevice.waitNotNull(
            Until.gone(By.text("UNDO")),
            waitingTime,
        )
        Log.i(TAG, "verifySnackBarHidden: Waited until undo snack bar button was gone")
        Log.i(TAG, "verifySnackBarHidden: Trying to verify bookmark snack bar does not exist")
        waitUntilSnackbarGone()
        Log.i(TAG, "verifySnackBarHidden: Verified bookmark snack bar does not exist")
    }

    fun verifyEditBookmarksView() =
        assertUIObjectExists(
            itemWithDescription("Navigate up"),
            itemWithText(getStringResource(R.string.edit_bookmark_fragment_title)),
            itemWithResId("$packageName:id/delete_bookmark_button"),
            itemWithResId("$packageName:id/save_bookmark_button"),
            itemWithResId("$packageName:id/bookmarkNameEdit"),
            itemWithResId("$packageName:id/bookmarkUrlEdit"),
            itemWithResId("$packageName:id/bookmarkParentFolderSelector"),
        )

    fun verifyRedesignedBookmarksMenuEditBookmarksView(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyRedesignedBookmarksMenuEditBookmarksView: Trying to verify that the edit bookmark view items are displayed")
        composeTestRule.onNodeWithContentDescription(getStringResource(R.string.bookmark_navigate_back_button_content_description))
            .assertIsDisplayed()
        composeTestRule.onNodeWithText(getStringResource(R.string.edit_bookmark_fragment_title))
            .assertIsDisplayed()
        composeTestRule.onNodeWithContentDescription(getStringResource(R.string.bookmark_delete_bookmark_content_description))
            .assertIsDisplayed()
        composeTestRule.onNodeWithTag(editBookmarkedItemTileTextField)
            .assertIsDisplayed()
        composeTestRule.onNodeWithTag(editBookmarkedItemURLTextField)
            .assertIsDisplayed()
        composeTestRule.onNodeWithText("Bookmarks").assertIsDisplayed()
        Log.i(TAG, "verifyRedesignedBookmarksMenuEditBookmarksView: Verified that the edit bookmark view items are displayed")
    }

    fun verifyKeyboardHidden(isExpectedToBeVisible: Boolean) {
        Log.i(TAG, "assertKeyboardVisibility: Trying to verify that the keyboard is visible: $isExpectedToBeVisible")
        assertEquals(
            isExpectedToBeVisible,
            mDevice
                .executeShellCommand("dumpsys input_method | grep mInputShown")
                .contains("mInputShown=true"),
        )
        Log.i(TAG, "assertKeyboardVisibility: Verified that the keyboard is visible: $isExpectedToBeVisible")
    }

    fun verifyShareOverlay() {
        Log.i(TAG, "verifyShareOverlay: Trying to verify bookmarks sharing overlay is displayed")
        onView(withId(R.id.shareWrapper)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareOverlay: Verified bookmarks sharing overlay is displayed")
    }

    fun verifyShareBookmarkFavicon() {
        Log.i(TAG, "verifyShareBookmarkFavicon: Trying to verify shared bookmarks favicon is displayed")
        onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareBookmarkFavicon: Verified shared bookmarks favicon is displayed")
    }

    fun verifyShareBookmarkTitle() {
        Log.i(TAG, "verifyShareBookmarkTitle: Trying to verify shared bookmarks title is displayed")
        onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareBookmarkTitle: Verified shared bookmarks title is displayed")
    }

    fun verifyShareBookmarkUrl() {
        Log.i(TAG, "verifyShareBookmarkUrl: Trying to verify shared bookmarks url is displayed")
        onView(withId(R.id.share_tab_url)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareBookmarkUrl: Verified shared bookmarks url is displayed")
    }

    fun verifyCurrentFolderTitle(title: String) {
        Log.i(TAG, "verifyCurrentFolderTitle: Waiting for $waitingTimeShort ms for bookmark with title: $title to exist")
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/navigationToolbar")
                .textContains(title),
        ).waitForExists(waitingTimeShort)
        Log.i(TAG, "verifyCurrentFolderTitle: Waited for $waitingTimeShort ms for bookmark with title: $title to exist")
        Log.i(TAG, "verifyCurrentFolderTitle: Trying to verify bookmark with title: $title is displayed")
        onView(
            allOf(
                withText(title),
                withParent(withId(R.id.navigationToolbar)),
            ),
        ).check(matches(isDisplayed()))
        Log.i(TAG, "verifyCurrentFolderTitle: Verified bookmark with title: $title is displayed")
    }

    fun waitForBookmarksFolderContentToExist(parentFolderName: String, childFolderName: String) {
        Log.i(TAG, "waitForBookmarksFolderContentToExist: Waiting for $waitingTimeShort ms for navigation toolbar containing bookmark folder with title: $parentFolderName to exist")
        itemWithResIdContainingText("$packageName:id/navigationToolbar", parentFolderName).waitForExists(waitingTimeShort)
        Log.i(TAG, "waitForBookmarksFolderContentToExist: Waited for $waitingTimeShort ms for navigation toolbar containing bookmark folder with title: $parentFolderName to exist")
        Log.i(TAG, "waitForBookmarksFolderContentToExist: Waiting for $waitingTimeShort ms for $childFolderName to exist")
        itemContainingText(childFolderName).waitForExists(waitingTimeShort)
        Log.i(TAG, "waitForBookmarksFolderContentToExist: Waited for $waitingTimeShort ms for $childFolderName to exist")
    }

    fun verifySyncSignInButton() {
        Log.i(TAG, "verifySyncSignInButton: Trying to verify sign in to sync button is visible")
        syncSignInButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifySyncSignInButton: Verified sign in to sync button is visible")
    }

    fun cancelFolderDeletion() {
        Log.i(TAG, "cancelFolderDeletion: Trying to click \"Cancel\" bookmarks folder deletion dialog button")
        onView(withText("CANCEL"))
            .inRoot(RootMatchers.isDialog())
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "cancelFolderDeletion: Clicked \"Cancel\" bookmarks folder deletion dialog button")
    }

    fun cancelDeletionFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "cancelDeletionFromRedesignedBookmarksMenu: Trying to click \"Cancel\" bookmarks folder deletion dialog button")
        composeTestRule.onNodeWithText(getStringResource(R.string.bookmark_delete_negative).uppercase()).performClick()
        Log.i(TAG, "cancelDeletionFromRedesignedBookmarksMenu: Clicked \"Cancel\" bookmarks folder deletion dialog button")
    }

    fun createFolder(name: String, parent: String? = null) {
        clickAddFolderButton()
        addNewFolderName(name)
        if (!parent.isNullOrBlank()) {
            setParentFolder(parent)
        }
        saveNewFolder()
    }

    fun createFolderFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, name: String, parent: String? = null) {
        clickAddFolderButtonFromRedesignedBookmarksMenu(composeTestRule)
        addNewFolderNameFromRedesignedBookmarksMenu(composeTestRule, name)
        if (!parent.isNullOrBlank()) {
            setParentFolderFromRedesignedBookmarksMenu(composeTestRule, parent)
        }
        saveNewFolderFromRedesignedBookmarksMenu(composeTestRule)
    }

    fun setParentFolder(parentName: String) {
        clickParentFolderSelector()
        selectFolder(parentName)
        navigateUp()
    }

    fun setParentFolderFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, parentName: String) {
        clickParentFolderSelectorFromRedesignedBookmarksMenu(composeTestRule)
        selectFolderFromRedesignedBookmarksMenu(composeTestRule, parentName)
        navigateUpFromRedesignedBookmarksMenu(composeTestRule)
    }

    fun clickAddFolderButton() {
        Log.i(TAG, "clickAddFolderButton: Trying to click add bookmarks folder button")
        addFolderButton().click()
        Log.i(TAG, "clickAddFolderButton: Clicked add bookmarks folder button")
    }

    fun clickAddFolderButtonFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "clickAddFolderButtonFromRedesignedBookmarks: Trying to click add bookmarks folder button")
        redesignedBookmarkMenuAddFolderButton(composeTestRule).performClick()
        Log.i(TAG, "clickAddFolderButtonFromRedesignedBookmarks: Clicked add bookmarks folder button")
        composeTestRule.waitForIdle()
    }

    fun clickAddNewFolderButtonFromSelectFolderView() {
        itemWithResId("$packageName:id/add_folder_button")
            .also {
                Log.i(TAG, "clickAddNewFolderButtonFromSelectFolderView: Waiting for $waitingTime ms for add bookmarks folder button from folder selection view to exist")
                it.waitForExists(waitingTime)
                Log.i(TAG, "clickAddNewFolderButtonFromSelectFolderView: Waited for $waitingTime ms for add bookmarks folder button from folder selection view to exist")
                Log.i(TAG, "clickAddNewFolderButtonFromSelectFolderView: Trying to click add bookmarks folder button from folder selection view")
                it.click()
                Log.i(TAG, "clickAddNewFolderButtonFromSelectFolderView: Clicked add bookmarks folder button from folder selection view")
            }
    }

    fun addNewFolderName(name: String) {
        Log.i(TAG, "addNewFolderName: Trying to click add folder name field")
        addFolderTitleField().click()
        Log.i(TAG, "addNewFolderName: Clicked to click add folder name field")
        Log.i(TAG, "addNewFolderName: Trying to set bookmarks folder name to: $name")
        addFolderTitleField().perform(replaceText(name))
        Log.i(TAG, "addNewFolderName: Bookmarks folder name was set to: $name")
    }

    fun addNewFolderNameFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, name: String) {
        Log.i(TAG, "addNewFolderNameFromRedesignedBookmarksMenu: Trying to set bookmarks folder name to: $name")
        redesignedBookmarkMenuAddFolderTitleField(composeTestRule).performTextInput(name)
        Log.i(TAG, "addNewFolderNameFromRedesignedBookmarksMenu: Bookmarks folder name was set to: $name")
    }

    fun saveNewFolder() {
        Log.i(TAG, "saveNewFolder: Trying to click save folder button")
        saveFolderButton().click()
        Log.i(TAG, "saveNewFolder: Clicked save folder button")
    }

    fun saveNewFolderFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "saveNewFolderFromRedesignedBookmarksMenu: Trying to click navigate up toolbar button")
        redesignedBookmarkMenuNavigateUpButton(composeTestRule).performClick()
        Log.i(TAG, "saveNewFolderFromRedesignedBookmarksMenu: Clicked the navigate up toolbar button")
    }

    fun navigateUp() {
        Log.i(TAG, "navigateUp: Trying to click navigate up toolbar button")
        goBackButton().click()
        Log.i(TAG, "navigateUp: Clicked navigate up toolbar button")
    }

    fun navigateUpFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "navigateUpFromRedesignedBookmarksMenu: Trying to click navigate up toolbar button")
        redesignedBookmarkMenuNavigateUpButton(composeTestRule).performClick()
        Log.i(TAG, "navigateUpFromRedesignedBookmarksMenu: Clicked navigate up toolbar button")
    }

    fun changeBookmarkTitle(newTitle: String) {
        Log.i(TAG, "changeBookmarkTitle: Trying to clear bookmark name text box")
        bookmarkNameEditBox().perform(clearText())
        Log.i(TAG, "changeBookmarkTitle: Cleared bookmark name text box")
        Log.i(TAG, "changeBookmarkTitle: Trying to set bookmark title to: $newTitle")
        bookmarkNameEditBox().perform(typeText(newTitle))
        Log.i(TAG, "changeBookmarkTitle: Bookmark title was set to: $newTitle")
    }

    fun changeBookmarkTitleFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, newTitle: String) {
        Log.i(TAG, "changeBookmarkTitleFromRedesignedBookmarksMenu: Trying to clear bookmark name text box")
        redesignedBookmarksMenuBookmarkNameEditBox(composeTestRule).performTextClearance()
        Log.i(TAG, "changeBookmarkTitleFromRedesignedBookmarksMenu: Cleared bookmark name text box")
        Log.i(TAG, "changeBookmarkTitleFromRedesignedBookmarksMenu: Trying to set bookmark title to: $newTitle")
        redesignedBookmarksMenuBookmarkNameEditBox(composeTestRule).performTextInput(newTitle)
        Log.i(TAG, "changeBookmarkTitleFromRedesignedBookmarksMenu: Bookmark title was set to: $newTitle")
    }

    fun changeBookmarkUrl(newUrl: String) {
        Log.i(TAG, "changeBookmarkUrl: Trying to clear bookmark url text box")
        bookmarkURLEditBox().perform(clearText())
        Log.i(TAG, "changeBookmarkUrl: Cleared bookmark url text box")
        Log.i(TAG, "changeBookmarkUrl: Trying to set bookmark url to: $newUrl")
        bookmarkURLEditBox().perform(typeText(newUrl))
        Log.i(TAG, "changeBookmarkUrl: Bookmark url was set to: $newUrl")
    }

    fun changeBookmarkUrlFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, newUrl: String) {
        Log.i(TAG, "changeBookmarkUrlFromRedesignedBookmarksMenu: Trying to clear bookmark url text box")
        redesignedBookmarksMenuBookmarkURLEditBox(composeTestRule).performTextClearance()
        Log.i(TAG, "changeBookmarkUrlFromRedesignedBookmarksMenu: Cleared bookmark url text box")
        Log.i(TAG, "changeBookmarkUrlFromRedesignedBookmarksMenu: Trying to set bookmark url to: $newUrl")
        redesignedBookmarksMenuBookmarkURLEditBox(composeTestRule).performTextInput(newUrl)
        Log.i(TAG, "changeBookmarkUrlFromRedesignedBookmarksMenu: Bookmark url was set to: $newUrl")
    }

    fun saveEditBookmark() {
        Log.i(TAG, "saveEditBookmark: Trying to click save bookmark button")
        saveBookmarkButton().click()
        Log.i(TAG, "saveEditBookmark: Clicked save bookmark button")
        Log.i(TAG, "saveEditBookmark: Waiting for $waitingTime ms for bookmarks list to exist")
        mDevice.findObject(UiSelector().resourceId("org.mozilla.fenix.debug:id/bookmark_list")).waitForExists(waitingTime)
        Log.i(TAG, "saveEditBookmark: Waited for $waitingTime ms for bookmarks list to exist")
    }

    fun saveEditBookmarkFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "saveEditBookmarkFromRedesignedBookmarksMenu: Trying to click navigate up toolbar button")
        redesignedBookmarkMenuNavigateUpButton(composeTestRule).performClick()
        Log.i(TAG, "saveEditBookmarkFromRedesignedBookmarksMenu: Clicked navigate up toolbar button")
    }

    fun clickParentFolderSelector() {
        Log.i(TAG, "clickParentFolderSelector: Trying to click folder selector")
        bookmarkFolderSelector().click()
        Log.i(TAG, "clickParentFolderSelector: Clicked folder selector")
    }

    fun clickParentFolderSelectorFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "clickParentFolderSelectorFromRedesignedBookmarksMenu: Trying to click folder selector")
        redesignedBookmarkMenuBookmarkFolderSelector(composeTestRule).performClick()
        Log.i(TAG, "clickParentFolderSelectorFromRedesignedBookmarksMenu: Clicked folder selector")
    }

    fun selectFolder(title: String) {
        Log.i(TAG, "selectFolder: Trying to click folder with title: $title")
        onView(withText(title)).click()
        Log.i(TAG, "selectFolder: Clicked folder with title: $title")
    }

    fun selectFolderFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, title: String) {
        Log.i(TAG, "selectFolderFromRedesignedBookmarksMenu: Trying to click folder with title: $title")
        composeTestRule.onNodeWithText(title).performClick()
        Log.i(TAG, "selectFolderFromRedesignedBookmarksMenu: Clicked folder with title: $title")
    }

    fun longTapDesktopFolder(title: String) {
        Log.i(TAG, "longTapDesktopFolder: Trying to long tap folder with title: $title")
        onView(withText(title)).perform(longClick())
        Log.i(TAG, "longTapDesktopFolder: Long tapped folder with title: $title")
    }

    fun longClickBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, title: String) {
        Log.i(TAG, "longClickBookmarkedItemFromRedesignedBookmarksMenu: Trying to long click bookmark with title: $title")
        composeTestRule.onNodeWithText(title).performTouchInput { longClick(durationMillis = LONG_CLICK_DURATION) }
        Log.i(TAG, "longClickBookmarkedItemFromRedesignedBookmarksMenu: Long clicked bookmark with title: $title")
    }

    fun selectBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, title: String) {
        Log.i(TAG, "selectBookmarkedItemFromRedesignedBookmarksMenu: Trying to click and select bookmark with title: $title")
        composeTestRule.onNodeWithText(title).performClick()
        Log.i(TAG, "selectBookmarkedItemFromRedesignedBookmarksMenu: Clicked and selected bookmark with title: $title")
    }

    fun cancelDeletion() {
        val cancelButton = mDevice.findObject(UiSelector().textContains("CANCEL"))
        Log.i(TAG, "cancelDeletion: Waiting for $waitingTime ms for \"Cancel\" bookmarks deletion button to exist")
        cancelButton.waitForExists(waitingTime)
        Log.i(TAG, "cancelDeletion: Waited for $waitingTime ms for \"Cancel\" bookmarks deletion button to exist")
        Log.i(TAG, "cancelDeletion: Trying to click \"Cancel\" bookmarks deletion button")
        cancelButton.click()
        Log.i(TAG, "cancelDeletion: Clicked \"Cancel\" bookmarks deletion button")
    }

    fun confirmDeletion() {
        Log.i(TAG, "confirmDeletion: Trying to click \"Delete\" bookmarks deletion button")
        onView(withText(R.string.delete_browsing_data_prompt_allow))
            .inRoot(RootMatchers.isDialog())
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "confirmDeletion: Clicked \"Delete\" bookmarks deletion button")
    }

    fun confirmDeletionFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "confirmDeletionFromRedesignedBookmarksMenu: Trying to click \"Delete\" bookmarks deletion button")
        composeTestRule.onNodeWithText("DELETE").performClick()
        Log.i(TAG, "confirmDeletionFromRedesignedBookmarksMenu: Clicked \"Delete\" bookmarks deletion button")
    }

    fun clickDeleteInEditModeButton() {
        Log.i(TAG, "clickDeleteInEditModeButton: Trying to click delete bookmarks button while in edit mode")
        deleteInEditModeButton().click()
        Log.i(TAG, "clickDeleteInEditModeButton: Clicked delete bookmarks button while in edit mode")
    }

    class Transition {
        fun closeMenu(interact: HomeScreenRobot.() -> Unit): Transition {
            Log.i(TAG, "closeMenu: Trying to click close bookmarks section button")
            closeButton().click()
            Log.i(TAG, "closeMenu: Clicked close bookmarks section button")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openThreeDotMenu(bookmark: String, interact: ThreeDotMenuBookmarksRobot.() -> Unit): ThreeDotMenuBookmarksRobot.Transition {
            mDevice.waitNotNull(Until.findObject(res("$packageName:id/overflow_menu")))
            Log.i(TAG, "openThreeDotMenu: Trying to click three dot button for bookmark item: $bookmark")
            threeDotMenu(bookmark).click()
            Log.i(TAG, "openThreeDotMenu: Clicked three dot button for bookmark item: $bookmark")

            ThreeDotMenuBookmarksRobot().interact()
            return ThreeDotMenuBookmarksRobot.Transition()
        }

        fun openThreeDotMenuFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, bookmarkedItem: String, interact: ThreeDotMenuBookmarksRobot.() -> Unit): ThreeDotMenuBookmarksRobot.Transition {
            Log.i(TAG, "openThreeDotMenu: Trying to click three dot button for bookmark item: $bookmarkedItem")
            redesignedBookmarkMenuBookmarkedItemThreeDotButton(composeTestRule, bookmarkedItem).performClick()
            Log.i(TAG, "openThreeDotMenu: Clicked three dot button for bookmark item: $bookmarkedItem")

            ThreeDotMenuBookmarksRobot().interact()
            return ThreeDotMenuBookmarksRobot.Transition()
        }

        fun clickSingInToSyncButton(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            Log.i(TAG, "clickSingInToSyncButton: Trying to click sign in to sync button")
            syncSignInButton().click()
            Log.i(TAG, "clickSingInToSyncButton: Clicked sign in to sync button")

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            Log.i(TAG, "goBack: Trying to click go back button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked go back button")

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBackToBrowserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "goBackToBrowserScreen: Trying to click go back button")
            goBackButton().click()
            Log.i(TAG, "goBackToBrowserScreen: Clicked go back button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeEditBookmarkSection(interact: BookmarksRobot.() -> Unit): Transition {
            Log.i(TAG, "goBackToBrowserScreen: Trying to click go back button")
            goBackButton().click()
            Log.i(TAG, "goBackToBrowserScreen: Clicked go back button")

            BookmarksRobot().interact()
            return Transition()
        }

        fun openBookmarkWithTitle(bookmarkTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            itemWithResIdAndText("$packageName:id/title", bookmarkTitle)
                .also {
                    Log.i(TAG, "openBookmarkWithTitle: Waiting for $waitingTime ms for bookmark with title: $bookmarkTitle")
                    it.waitForExists(waitingTime)
                    Log.i(TAG, "openBookmarkWithTitle: Waited for $waitingTime ms for bookmark with title: $bookmarkTitle")
                    Log.i(TAG, "openBookmarkWithTitle: Trying to click bookmark with title: $bookmarkTitle and wait for $waitingTimeShort ms for a new window")
                    it.clickAndWaitForNewWindow(waitingTimeShort)
                    Log.i(TAG, "openBookmarkWithTitle: Clicked bookmark with title: $bookmarkTitle and waited for $waitingTimeShort ms for a new window")
                }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openBookmarkWithTitleFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, bookmarkTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openBookmarkWithTitleFromRedesignedBookmarksMenu: Trying to click bookmark with title: $bookmarkTitle")
            composeTestRule.onNodeWithText(bookmarkTitle).performClick()
            Log.i(TAG, "openBookmarkWithTitleFromRedesignedBookmarksMenu: Clicked bookmark with title: $bookmarkTitle")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSearchButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "clickSearchButton: Trying to click search bookmarks button")
            itemWithResId("$packageName:id/bookmark_search").click()
            Log.i(TAG, "clickSearchButton: Clicked search bookmarks button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        @OptIn(ExperimentalTestApi::class)
        fun clickSearchButtonFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "clickSearchButtonFromRedesignedBookmarksMenu: Waiting for the search bookmarks button to exist")
            composeTestRule.waitUntilAtLeastOneExists(hasContentDescription(getStringResource(R.string.bookmark_search_button_content_description)))
            Log.i(TAG, "clickSearchButtonFromRedesignedBookmarksMenu: Waiting for the search bookmarks button to exist")
            Log.i(TAG, "clickSearchButtonFromRedesignedBookmarksMenu: Trying to click search bookmarks button")
            composeTestRule.onNodeWithContentDescription(getStringResource(R.string.bookmark_search_button_content_description)).performClick()
            Log.i(TAG, "clickSearchButtonFromRedesignedBookmarksMenu: Clicked search bookmarks button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

fun bookmarksMenu(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
    BookmarksRobot().interact()
    return BookmarksRobot.Transition()
}

private fun closeButton() = onView(withId(R.id.close_bookmarks))

private fun goBackButton() = onView(withContentDescription("Navigate up"))

private fun bookmarkFavicon(url: String) = onView(
    allOf(
        withId(R.id.favicon),
        withParent(
            withParent(
                withChild(allOf(withId(R.id.url), withText(url))),
            ),
        ),
    ),
)

private fun bookmarkURL(url: String) = onView(allOf(withId(R.id.url), withText(containsString(url))))

private fun addFolderButton() = onView(withId(R.id.add_bookmark_folder))

private fun redesignedBookmarkMenuAddFolderButton(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithContentDescription(getStringResource(R.string.bookmark_select_folder_new_folder_button_title))

private fun addFolderTitleField() = onView(withId(R.id.bookmarkNameEdit))

private fun redesignedBookmarkMenuAddFolderTitleField(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithTag(addBookmarkFolderNameTextField).onChildAt(0)

private fun saveFolderButton() = onView(withId(R.id.confirm_add_folder_button))

private fun redesignedBookmarkMenuNavigateUpButton(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithContentDescription(getStringResource(R.string.bookmark_navigate_back_button_content_description))

private fun threeDotMenu(bookmark: String) = onView(
    allOf(
        withId(R.id.overflow_menu),
        hasSibling(withText(bookmark)),
    ),
)

private fun redesignedBookmarkMenuBookmarkedItemThreeDotButton(composeTestRule: ComposeTestRule, bookmarkedItem: String) =
    composeTestRule.onNodeWithContentDescription("Item Menu for $bookmarkedItem")

private fun bookmarkNameEditBox() = onView(withId(R.id.bookmarkNameEdit))

private fun redesignedBookmarksMenuBookmarkNameEditBox(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithTag(editBookmarkedItemTileTextField).onChildAt(0)

private fun bookmarkFolderSelector() = onView(withId(R.id.bookmarkParentFolderSelector))

private fun redesignedBookmarkMenuBookmarkFolderSelector(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithText("Bookmarks")

private fun bookmarkURLEditBox() = onView(withId(R.id.bookmarkUrlEdit))

private fun redesignedBookmarksMenuBookmarkURLEditBox(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithTag(editBookmarkedItemURLTextField).onChildAt(0)

private fun saveBookmarkButton() = onView(withId(R.id.save_bookmark_button))

private fun deleteInEditModeButton() = onView(withId(R.id.delete_bookmark_button))

private fun syncSignInButton() = onView(withId(R.id.bookmark_folders_sign_in))
