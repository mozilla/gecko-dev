package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createBookmarkItem
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.composeBookmarksMenu
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.multipleSelectionToolbar

class BookmarksTestCompose : TestSetup() {
    private val testBookmark = object {
        var title: String = "Bookmark title"
        var url: String = "https://www.example.com/"
    }

    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                isNewBookmarksEnabled = true,
                isNavigationToolbarEnabled = false,
                isNavigationBarCFREnabled = false,
                isSetAsDefaultBrowserPromptEnabled = false,
                isMenuRedesignEnabled = false,
                isMenuRedesignCFREnabled = false,
                shouldUseBottomToolbar = true,
            ),
        ) { it.activity }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2833690
    @SmokeTest
    @Test
    fun deleteBookmarkFoldersTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(website.url)
        }.openThreeDotMenu {
        }.openBookmarksMenu(composeTestRule) {
            verifyBookmarkTitle("Test_Page_1")
            createFolder("My Folder")
            verifyFolderTitle("My Folder")
        }.openThreeDotMenu("Test_Page_1") {
        }.clickEdit {
            clickParentFolderSelector()
            selectFolder("My Folder")
            navigateUp()
            saveEditBookmark()
            createFolder("My Folder 2")
            verifyFolderTitle("My Folder 2")
        }.openThreeDotMenu("My Folder 2") {
        }.clickEdit {
            clickParentFolderSelector()
            selectFolder("My Folder")
            navigateUp()
            saveEditBookmark()
        }.openThreeDotMenu("My Folder") {
        }.clickDelete {
            cancelFolderDeletion()
            verifyFolderTitle("My Folder")
        }.openThreeDotMenu("My Folder") {
        }.clickDelete {
            confirmDeletion()
            verifyBookmarkIsDeleted("My Folder")
            verifyBookmarkIsDeleted("My Folder 2")
            verifyBookmarkIsDeleted("Test_Page_1")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2833691
    @SmokeTest
    @Test
    fun editBookmarksNameAndUrlTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.editBookmarkPage(composeTestRule) {
            verifyEditBookmarksView()
            changeBookmarkTitle(testBookmark.title)
            changeBookmarkUrl(testBookmark.url)
            saveEditBookmark()
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openBookmarksMenu(composeTestRule) {
            verifyBookmarkTitle(testBookmark.title)
            verifyBookmarkedURL("https://www.example.com/")
        }.openBookmarkWithTitle(testBookmark.title) {
            verifyUrl("example.com")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2833693
    @SmokeTest
    @Test
    fun shareBookmarkTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(defaultWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarksMenu(composeTestRule) {
        }.openThreeDotMenu(defaultWebPage.title) {
        }.clickShare {
            verifyShareTabLayout()
            verifySharingWithSelectedApp(
                appName = "Gmail",
                content = defaultWebPage.url.toString(),
                subject = defaultWebPage.title,
            )
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2833702
    @SmokeTest
    @Test
    fun openMultipleSelectedBookmarksInANewTabTest() {
        val webPages = listOf(
            TestAssetHelper.getGenericAsset(mockWebServer, 1),
            TestAssetHelper.getGenericAsset(mockWebServer, 2),
        )

        createBookmarkItem(webPages[0].url.toString(), webPages[0].title, null)
        createBookmarkItem(webPages[1].url.toString(), webPages[1].title, null)

        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarksMenu(composeTestRule) {
            longClickBookmarkedItem(webPages[0].title)
            selectBookmarkedItem(webPages[1].title)
        }

        multipleSelectionToolbar {
            clickMultiSelectThreeDotButton(composeTestRule)
        }.clickOpenInNewTabButton(composeTestRule) {
            verifyTabTrayIsOpen()
            verifyNormalBrowsingButtonIsSelected()
            verifyNormalTabsList()
            verifyExistingOpenTabs(webPages[0].url.toString(), webPages[1].url.toString())
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2833704
    @SmokeTest
    @Test
    fun deleteMultipleSelectedBookmarksTest() {
        val webPages = listOf(
            TestAssetHelper.getGenericAsset(mockWebServer, 1),
            TestAssetHelper.getGenericAsset(mockWebServer, 2),
        )

        createBookmarkItem(webPages[0].url.toString(), webPages[0].title, null)
        createBookmarkItem(webPages[1].url.toString(), webPages[1].title, null)

        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarksMenu(composeTestRule) {
            longClickBookmarkedItem(webPages[0].title)
            selectBookmarkedItem(webPages[1].title)
        }

        multipleSelectionToolbar {
            clickMultiSelectThreeDotButton(composeTestRule)
            clickMultiSelectDeleteButton(composeTestRule)
        }

        composeBookmarksMenu(composeTestRule) {
            cancelFolderDeletion()
            verifyBookmarkTitle(webPages[0].title)
            verifyBookmarkTitle(webPages[1].title)
            longClickBookmarkedItem(webPages[0].title)
            selectBookmarkedItem(webPages[1].title)
        }

        multipleSelectionToolbar {
            clickMultiSelectThreeDotButton(composeTestRule)
            clickMultiSelectDeleteButton(composeTestRule)
        }

        composeBookmarksMenu(composeTestRule) {
            confirmDeletion()
            verifyBookmarkIsDeleted(webPages[0].title)
            verifyBookmarkIsDeleted(webPages[1].title)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2833712
    @SmokeTest
    @Test
    fun verifySearchForBookmarkedItemsTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarksMenu(composeTestRule) {
            createFolder("My Folder")
            navigateUp()
        }

        browserScreen {
            createBookmark(composeTestRule, firstWebPage.url, "My Folder")
            createBookmark(composeTestRule, secondWebPage.url)
        }.openThreeDotMenu {
        }.openBookmarksMenu(composeTestRule) {
        }.clickSearchButton() {
            // Search for a valid term
            typeSearch(firstWebPage.title)
            verifySearchEngineSuggestionResults(composeTestRule, firstWebPage.url.toString(), searchTerm = firstWebPage.title)
            verifySuggestionsAreNotDisplayed(composeTestRule, secondWebPage.url.toString())
            // Search for invalid term
            typeSearch("Android")
            verifySuggestionsAreNotDisplayed(composeTestRule, firstWebPage.url.toString())
            verifySuggestionsAreNotDisplayed(composeTestRule, secondWebPage.url.toString())
        }
    }
}
