package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createBookmarkItem
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.bookmarksMenu
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.multipleSelectionToolbar

class ComposeBookmarksTest : TestSetup() {
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
        }.openRedesignedBookmarksMenu {
            verifyRedesignedBookmarkTitle(composeTestRule, "Test_Page_1")
            createFolderFromRedesignedBookmarksMenu(composeTestRule, "My Folder")
            verifyFolderTitleFromRedesignedBookmarksMenu(composeTestRule, "My Folder")
        }.openThreeDotMenuFromRedesignedBookmarksMenu(composeTestRule, "Test_Page_1") {
        }.clickEditFromRedesignedBookmarksMenu(composeTestRule) {
            clickParentFolderSelectorFromRedesignedBookmarksMenu(composeTestRule)
            selectFolderFromRedesignedBookmarksMenu(composeTestRule, "My Folder")
            navigateUpFromRedesignedBookmarksMenu(composeTestRule)
            saveEditBookmarkFromRedesignedBookmarksMenu(composeTestRule)
            createFolderFromRedesignedBookmarksMenu(composeTestRule, "My Folder 2")
            verifyFolderTitleFromRedesignedBookmarksMenu(composeTestRule, "My Folder 2")
        }.openThreeDotMenuFromRedesignedBookmarksMenu(composeTestRule, "My Folder 2") {
        }.clickEditFromRedesignedBookmarksMenu(composeTestRule) {
            clickParentFolderSelectorFromRedesignedBookmarksMenu(composeTestRule)
            selectFolderFromRedesignedBookmarksMenu(composeTestRule, "My Folder")
            navigateUpFromRedesignedBookmarksMenu(composeTestRule)
            saveEditBookmarkFromRedesignedBookmarksMenu(composeTestRule)
        }.openThreeDotMenuFromRedesignedBookmarksMenu(composeTestRule, "My Folder") {
        }.clickDeleteFromRedesignedBookmarksMenu(composeTestRule) {
            cancelDeletionFromRedesignedBookmarksMenu(composeTestRule)
            verifyFolderTitleFromRedesignedBookmarksMenu(composeTestRule, "My Folder")
        }.openThreeDotMenuFromRedesignedBookmarksMenu(composeTestRule, "My Folder") {
        }.clickDeleteFromRedesignedBookmarksMenu(composeTestRule) {
            confirmDeletionFromRedesignedBookmarksMenu(composeTestRule)
            verifyBookmarkIsDeletedFromRedesignedBookmarksMenu(composeTestRule, "My Folder")
            verifyBookmarkIsDeletedFromRedesignedBookmarksMenu(composeTestRule, "My Folder 2")
            verifyBookmarkIsDeletedFromRedesignedBookmarksMenu(composeTestRule, "Test_Page_1")
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
        }.editBookmarkPage {
            verifyRedesignedBookmarksMenuEditBookmarksView(composeTestRule)
            changeBookmarkTitleFromRedesignedBookmarksMenu(composeTestRule, testBookmark.title)
            changeBookmarkUrlFromRedesignedBookmarksMenu(composeTestRule, testBookmark.url)
            saveEditBookmarkFromRedesignedBookmarksMenu(composeTestRule)
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openRedesignedBookmarksMenu {
            verifyRedesignedBookmarkTitle(composeTestRule, testBookmark.title)
            verifyRedesignedBookmarkURL(composeTestRule, "https://www.example.com/")
        }.openBookmarkWithTitleFromRedesignedBookmarksMenu(composeTestRule, testBookmark.title) {
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
        }.openRedesignedBookmarksMenu {
        }.openThreeDotMenuFromRedesignedBookmarksMenu(composeTestRule, defaultWebPage.title) {
        }.clickShareFromRedesignedBookmarksMenu(composeTestRule) {
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
        }.openRedesignedBookmarksMenu {
            longClickBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule, webPages[0].title)
            selectBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule, webPages[1].title)
        }

        multipleSelectionToolbar {
            clickRedesignedBookmarksMenuMultiSelectThreeDotButton(composeTestRule)
        }.clickOpenInNewTabButtonFromRedesignedBookmarksMenu(composeTestRule) {
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
        }.openRedesignedBookmarksMenu {
            longClickBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule, webPages[0].title)
            selectBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule, webPages[1].title)
        }

        multipleSelectionToolbar {
            clickRedesignedBookmarksMenuMultiSelectThreeDotButton(composeTestRule)
            clickRedesignedBookmarksMenuMultiSelectDeleteButton(composeTestRule)
        }

        bookmarksMenu {
            cancelDeletionFromRedesignedBookmarksMenu(composeTestRule)
            verifyRedesignedBookmarkTitle(composeTestRule, webPages[0].title)
            verifyRedesignedBookmarkTitle(composeTestRule, webPages[1].title)
            longClickBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule, webPages[0].title)
            selectBookmarkedItemFromRedesignedBookmarksMenu(composeTestRule, webPages[1].title)
        }

        multipleSelectionToolbar {
            clickRedesignedBookmarksMenuMultiSelectThreeDotButton(composeTestRule)
            clickRedesignedBookmarksMenuMultiSelectDeleteButton(composeTestRule)
        }

        bookmarksMenu {
            confirmDeletionFromRedesignedBookmarksMenu(composeTestRule)
            verifyBookmarkIsDeletedFromRedesignedBookmarksMenu(composeTestRule, webPages[0].title)
            verifyBookmarkIsDeletedFromRedesignedBookmarksMenu(composeTestRule, webPages[1].title)
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
        }.openRedesignedBookmarksMenu {
            createFolderFromRedesignedBookmarksMenu(composeTestRule, "My Folder")
            navigateUpFromRedesignedBookmarksMenu(composeTestRule)
        }

        browserScreen {
            createBookmarkFromRedesignedBookmarksMenu(composeTestRule, firstWebPage.url, "My Folder")
            createBookmarkFromRedesignedBookmarksMenu(composeTestRule, secondWebPage.url)
        }.openThreeDotMenu {
        }.openRedesignedBookmarksMenu {
        }.clickSearchButtonFromRedesignedBookmarksMenu(composeTestRule) {
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
