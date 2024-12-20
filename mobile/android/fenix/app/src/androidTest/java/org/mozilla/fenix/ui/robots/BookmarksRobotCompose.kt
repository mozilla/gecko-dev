package org.mozilla.fenix.ui.robots

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
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.library.bookmarks.BookmarksTestTag.addBookmarkFolderNameTextField
import org.mozilla.fenix.library.bookmarks.BookmarksTestTag.editBookmarkedItemTileTextField
import org.mozilla.fenix.library.bookmarks.BookmarksTestTag.editBookmarkedItemURLTextField

class BookmarksRobotCompose(private val composeTestRule: ComposeTestRule) {

    fun verifyBookmarkedURL(url: String) {
        Log.i(TAG, "verifyBookmarkedURL: Trying to verify bookmarks url: $url is displayed")
        composeTestRule.onNodeWithText(url).assertIsDisplayed()
        Log.i(TAG, "verifyBookmarkedURL: Verified bookmarks url: $url is displayed")
    }

    fun verifyFolderTitle(title: String) {
        Log.i(TAG, "verifyFolderTitle: Trying to verify bookmarks folder with title: $title is displayed")
        composeTestRule.onNodeWithText(title).assertIsDisplayed()
        Log.i(TAG, "verifyFolderTitle: Verified bookmarks folder with title: $title is displayed")
    }

    fun verifyBookmarkTitle(title: String) {
        Log.i(TAG, "verifyBookmarkTitle: Trying to verify bookmark with title: $title is displayed")
        composeTestRule.onNodeWithText(title).assertIsDisplayed()
        Log.i(TAG, "verifyBookmarkTitle: Verified bookmark with title: $title is displayed")
    }

    fun verifyBookmarkIsDeleted(expectedTitle: String) {
        Log.i(TAG, "verifyBookmarkIsDeleted: Trying to verify that the bookmarked item : $expectedTitle is not displayed")
        composeTestRule.onNodeWithText(expectedTitle).assertIsNotDisplayed()
        Log.i(TAG, "verifyBookmarkIsDeleted: Verified that the bookmarked item : $expectedTitle is not displayed")
    }

    fun verifyEditBookmarksView() {
        Log.i(TAG, "verifyEditBookmarksView: Trying to verify that the edit bookmark view items are displayed")
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
        Log.i(TAG, "verifyEditBookmarksView: Verified that the edit bookmark view items are displayed")
    }

    fun cancelFolderDeletion() {
        Log.i(TAG, "cancelFolderDeletion: Trying to click \"Cancel\" bookmarks folder deletion dialog button")
        composeTestRule.onNodeWithText(getStringResource(R.string.bookmark_delete_negative).uppercase()).performClick()
        Log.i(TAG, "cancelFolderDeletion: Clicked \"Cancel\" bookmarks folder deletion dialog button")
    }

    fun createFolder(name: String, parent: String? = null) {
        clickAddFolderButton()
        addNewFolderName(name)
        if (!parent.isNullOrBlank()) {
            setParentFolder(parent)
        }
        saveNewFolder()
    }

    fun setParentFolder(parentName: String) {
        clickParentFolderSelector()
        selectFolder(parentName)
        navigateUp()
    }

    fun clickAddFolderButton() {
        Log.i(TAG, "clickAddFolderButton: Trying to click add bookmarks folder button")
        composeTestRule.addFolderButton().performClick()
        Log.i(TAG, "clickAddFolderButton: Clicked add bookmarks folder button")
        composeTestRule.waitForIdle()
    }

    fun addNewFolderName(name: String) {
        Log.i(TAG, "addNewFolderName: Trying to set bookmarks folder name to: $name")
        composeTestRule.addFolderTitleField().performTextInput(name)
        Log.i(TAG, "addNewFolderName: Bookmarks folder name was set to: $name")
    }

    fun saveNewFolder() {
        Log.i(TAG, "saveNewFolder: Trying to click navigate up toolbar button")
        composeTestRule.navigateUpButton().performClick()
        Log.i(TAG, "saveNewFolder: Clicked the navigate up toolbar button")
    }

    fun navigateUp() {
        Log.i(TAG, "navigateUp: Trying to click navigate up toolbar button")
        composeTestRule.navigateUpButton().performClick()
        Log.i(TAG, "navigateUp: Clicked navigate up toolbar button")
    }

    fun changeBookmarkTitle(newTitle: String) {
        Log.i(TAG, "changeBookmarkTitle: Trying to clear bookmark name text box")
        composeTestRule.bookmarkNameEditBox().performTextClearance()
        Log.i(TAG, "changeBookmarkTitle: Cleared bookmark name text box")
        Log.i(TAG, "changeBookmarkTitle: Trying to set bookmark title to: $newTitle")
        composeTestRule.bookmarkNameEditBox().performTextInput(newTitle)
        Log.i(TAG, "changeBookmarkTitle: Bookmark title was set to: $newTitle")
    }

    fun changeBookmarkUrl(newUrl: String) {
        Log.i(TAG, "changeBookmarkUrl: Trying to clear bookmark url text box")
        composeTestRule.bookmarkURLEditBox().performTextClearance()
        Log.i(TAG, "changeBookmarkUrl: Cleared bookmark url text box")
        Log.i(TAG, "changeBookmarkUrl: Trying to set bookmark url to: $newUrl")
        composeTestRule.bookmarkURLEditBox().performTextInput(newUrl)
        Log.i(TAG, "changeBookmarkUrl: Bookmark url was set to: $newUrl")
    }

    fun saveEditBookmark() {
        Log.i(TAG, "saveEditBookmark: Trying to click navigate up toolbar button")
        composeTestRule.navigateUpButton().performClick()
        Log.i(TAG, "saveEditBookmark: Clicked navigate up toolbar button")
    }

    fun clickParentFolderSelector() {
        Log.i(TAG, "clickParentFolderSelector: Trying to click folder selector")
        composeTestRule.bookmarkFolderSelector().performClick()
        Log.i(TAG, "clickParentFolderSelector: Clicked folder selector")
    }

    fun selectFolder(title: String) {
        Log.i(TAG, "selectFolder: Trying to click folder with title: $title")
        composeTestRule.onNodeWithText(title).performClick()
        Log.i(TAG, "selectFolder: Clicked folder with title: $title")
    }

    fun longClickBookmarkedItem(title: String) {
        Log.i(TAG, "longClickBookmarkedItem: Trying to long click bookmark with title: $title")
        composeTestRule.onNodeWithText(title).performTouchInput { longClick(durationMillis = LONG_CLICK_DURATION) }
        Log.i(TAG, "longClickBookmarkedItem: Long clicked bookmark with title: $title")
    }

    fun selectBookmarkedItem(title: String) {
        Log.i(TAG, "selectBookmarkedItem: Trying to click and select bookmark with title: $title")
        composeTestRule.onNodeWithText(title).performClick()
        Log.i(TAG, "selectBookmarkedItem: Clicked and selected bookmark with title: $title")
    }

    fun confirmDeletion() {
        Log.i(TAG, "confirmDeletion: Trying to click \"Delete\" bookmarks deletion button")
        composeTestRule.onNodeWithText("DELETE").performClick()
        Log.i(TAG, "confirmDeletion: Clicked \"Delete\" bookmarks deletion button")
    }

    class Transition(private val composeTestRule: ComposeTestRule) {
        fun openThreeDotMenu(bookmarkedItem: String, interact: ThreeDotMenuBookmarksRobotCompose.() -> Unit): ThreeDotMenuBookmarksRobotCompose.Transition {
            Log.i(TAG, "openThreeDotMenu: Trying to click three dot button for bookmark item: $bookmarkedItem")
            composeTestRule.threeDotMenuButton(bookmarkedItem).performClick()
            Log.i(TAG, "openThreeDotMenu: Clicked three dot button for bookmark item: $bookmarkedItem")

            ThreeDotMenuBookmarksRobotCompose().interact()
            return ThreeDotMenuBookmarksRobotCompose.Transition(composeTestRule)
        }

        fun openBookmarkWithTitle(bookmarkTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openBookmarkWithTitle: Trying to click bookmark with title: $bookmarkTitle")
            composeTestRule.onNodeWithText(bookmarkTitle).performClick()
            Log.i(TAG, "openBookmarkWithTitle: Clicked bookmark with title: $bookmarkTitle")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        @OptIn(ExperimentalTestApi::class)
        fun clickSearchButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "clickSearchButton: Waiting for the search bookmarks button to exist")
            composeTestRule.waitUntilAtLeastOneExists(hasContentDescription(getStringResource(R.string.bookmark_search_button_content_description)))
            Log.i(TAG, "clickSearchButton: Waiting for the search bookmarks button to exist")
            Log.i(TAG, "clickSearchButton: Trying to click search bookmarks button")
            composeTestRule.onNodeWithContentDescription(getStringResource(R.string.bookmark_search_button_content_description)).performClick()
            Log.i(TAG, "clickSearchButton: Clicked search bookmarks button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

fun composeBookmarksMenu(composeTestRule: ComposeTestRule, interact: BookmarksRobotCompose.() -> Unit): BookmarksRobotCompose.Transition {
    BookmarksRobotCompose(composeTestRule).interact()
    return BookmarksRobotCompose.Transition(composeTestRule)
}

private fun ComposeTestRule.addFolderButton() =
    onNodeWithContentDescription(getStringResource(R.string.bookmark_select_folder_new_folder_button_title))

private fun ComposeTestRule.addFolderTitleField() =
    onNodeWithTag(addBookmarkFolderNameTextField).onChildAt(0)

private fun ComposeTestRule.navigateUpButton() =
    onNodeWithContentDescription(getStringResource(R.string.bookmark_navigate_back_button_content_description))

private fun ComposeTestRule.threeDotMenuButton(bookmarkedItem: String) =
    onNodeWithContentDescription("Item Menu for $bookmarkedItem")

private fun ComposeTestRule.bookmarkNameEditBox() =
    onNodeWithTag(editBookmarkedItemTileTextField).onChildAt(0)

private fun ComposeTestRule.bookmarkFolderSelector() =
    onNodeWithText("Bookmarks")

private fun ComposeTestRule.bookmarkURLEditBox() =
    onNodeWithTag(editBookmarkedItemURLTextField).onChildAt(0)
