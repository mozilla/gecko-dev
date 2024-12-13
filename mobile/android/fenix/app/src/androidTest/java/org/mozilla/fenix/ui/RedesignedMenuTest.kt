/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import androidx.test.rule.ActivityTestRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertAppWithPackageNameOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertYoutubeAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.clickSystemHomeScreenShortcutAddButton
import org.mozilla.fenix.helpers.DataGenerationHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestHelper.waitUntilSnackbarGone
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class RedesignedMenuTest : TestSetup() {
    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                skipOnboarding = true,
                isNavigationToolbarEnabled = true,
                isNavigationBarCFREnabled = false,
                isSetAsDefaultBrowserPromptEnabled = false,
                isMenuRedesignEnabled = true,
                isMenuRedesignCFREnabled = false,
            ),
        ) { it.activity }

    @get: Rule
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java,
        true,
        false,
    )

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2763136
    @SmokeTest
    @Test
    fun homepageRedesignedMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenuFromRedesignedToolbar() {
            expandRedesignedMenu(composeTestRule)
            verifyHomeRedesignedMainMenuItems(composeTestRule, false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769372
    @SmokeTest
    @Test
    fun webpageRedesignedMenuItemsTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            verifyPageMainMenuItems(composeTestRule, false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769377
    @SmokeTest
    @Test
    fun verifyTheNewTabButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
        }.clickNewTabButton(composeTestRule) {
            verifySearchToolbar(isDisplayed = true)
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769378
    @SmokeTest
    @Test
    fun verifyTheNewPrivateTabButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
        }.clickNewPrivateTabButton(composeTestRule) {
            verifySearchToolbar(isDisplayed = true)
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2766734
    @SmokeTest
    @Test
    fun verifySwitchToDesktopSiteIsDisabledOnPDFsTest() {
        val pdfPage = TestAssetHelper.getPdfFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pdfPage.url) {
            verifyPageContent(pdfPage.content)
        }.openThreeDotMenuFromRedesignedToolbar {
            verifySwitchToDesktopSiteButtonIsEnabled(composeTestRule, isEnabled = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2698232
    @SmokeTest
    @Test
    fun findInPageTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenuFromRedesignedToolbar {
        }.clickFindInPageButton(composeTestRule) {
            verifyFindInPageNextButton()
            verifyFindInPagePrevButton()
            verifyFindInPageCloseButton()
            enterFindInPageQuery("a")
            verifyFindInPageResult("1/3")
            clickFindInPageNextButton()
            verifyFindInPageResult("2/3")
            clickFindInPageNextButton()
            verifyFindInPageResult("3/3")
            clickFindInPagePrevButton()
            verifyFindInPageResult("2/3")
            clickFindInPagePrevButton()
            verifyFindInPageResult("1/3")
        }.closeFindInPageWithCloseButton {
            verifyFindInPageBar(false)
        }.openThreeDotMenuFromRedesignedToolbar {
        }.clickFindInPageButton(composeTestRule) {
            enterFindInPageQuery("3")
            verifyFindInPageResult("1/1")
        }.closeFindInPageWithBackButton {
            verifyFindInPageBar(false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769380
    @SmokeTest
    @Test
    fun verifyBookmarksMenuButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
        }.openBookmarks(composeTestRule) {
            verifyBookmarksMenuView()
        }.goBackToBrowserScreen {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769381
    @SmokeTest
    @Test
    fun verifyHistoryMenuButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
        }.openHistory(composeTestRule) {
            verifyHistoryMenuView()
        }.goBack {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769382
    @SmokeTest
    @Test
    fun verifyDownloadsMenuButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
        }.openDownloads(composeTestRule) {
            verifyEmptyDownloadsList(composeTestRule)
            TestHelper.exitMenu()
        }
        browserScreen {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769383
    @SmokeTest
    @Test
    fun verifyPasswordsMenuButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
        }.openPasswords(composeTestRule) {
            verifySecurityPromptForLogins()
            tapSetupLater()
            verifyEmptySavedLoginsListView()
            TestHelper.exitMenu()
        }
        browserScreen {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2763386
    // Verifies the main menu of a custom tab with a custom menu item
    @SmokeTest
    @Test
    fun verifyCustomTabMenuItemsTest() {
        val customMenuItem = "TestMenuItem"
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenuFromRedesignedToolbar {
            verifyRedesignedCustomTabsMainMenuItems(customMenuItem)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769619
    // The test opens a link in a custom tab then sends it to the browser
    @SmokeTest
    @Test
    fun openCustomTabInFirefoxTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenuFromRedesignedToolbar {
        }.clickOpenInBrowserButtonFromRedesignedToolbar {
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769620
    @SmokeTest
    @Test
    fun shareCustomTabUsingMainMenuButtonTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
        }.openMainMenuFromRedesignedToolbar {
        }.clickShareButtonFromRedesignedMenu {
            verifyShareTabLayout()
            verifySharingWithSelectedApp(
                appName = "Gmail",
                content = customTabPage.url.toString(),
                subject = customTabPage.title,
            )
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2777426
    @SmokeTest
    @Test
    fun verifyRecommendedExtensionsListTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
        }.openNoExtensionsMenuFromRedesignedMainMenu(composeTestRule) {
            verifyRecommendedAddonsViewFromRedesignedMainMenu(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2773811
    @SmokeTest
    @Test
    fun verifyRedesignedMenuAfterDisablingAnExtensionTest() {
        val addonName = "uBlock Origin"
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenuFromRedesignedToolbar {
        }.openNoExtensionsMenuFromRedesignedMainMenu(composeTestRule) {
            waitForAddonsListProgressBarToBeGone()
            clickInstallAddon(addonName)
            verifyAddonPermissionPrompt(addonName)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompleted(addonName, composeTestRule.activityRule)
            verifyAddonInstallCompletedPrompt(addonName)
            closeAddonInstallCompletePrompt()
        }.goBack {
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
        }.openPageViewExtensionsMenuFromRedesignedMainMenu(composeTestRule, "$addonName (0)") {
            clickManageExtensionsButtonFromRedesignedMainMenu(composeTestRule)
        }.openDetailedMenuForAddon(addonName) {
        }.removeAddon(composeTestRule.activityRule) {
            verifySnackBarText("Successfully uninstalled $addonName")
            waitUntilSnackbarGone()
        }.goBack {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            verifyNoExtensionsButton(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776938
    @SmokeTest
    @Test
    fun verifyTheManageExtensionsSubMenuTest() {
        val addonName = "uBlock Origin"
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenuFromRedesignedToolbar {
        }.openNoExtensionsMenuFromRedesignedMainMenu(composeTestRule) {
            waitForAddonsListProgressBarToBeGone()
            clickInstallAddon(addonName)
            verifyAddonPermissionPrompt(addonName)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompleted(addonName, composeTestRule.activityRule)
            verifyAddonInstallCompletedPrompt(addonName)
            closeAddonInstallCompletePrompt()
        }.goBack {
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
        }.openPageViewExtensionsMenuFromRedesignedMainMenu(composeTestRule, "$addonName (0)") {
            clickManageExtensionsButtonFromRedesignedMainMenu(composeTestRule)
        }.goBack {
        }
        browserScreen {
            verifyPageContent(genericURL.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776966
    @SmokeTest
    @Test
    fun verifyTheSaveSubMenuItemsTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
            verifySaveSubMenuItems(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776967
    @SmokeTest
    @Test
    fun verifyTheBookmarkThisPageSubMenuItemsTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickBookmarkThisPageButton(composeTestRule) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickEditBookmarkButton(composeTestRule) {
            verifyEditBookmarksView()
            clickDeleteInEditModeButton()
            confirmDeletion()
        }
        browserScreen {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
            verifyBookmarkThisPageButton(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776968
    @SmokeTest
    @Test
    fun verifyTheAddToShortcutsSubMenuOptionTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickAddToShortcutsButton(composeTestRule) {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreenWithRedesignedToolbar {
            verifyExistingTopSitesTabs(testPage.title)
        }.openTopSiteTabWithTitle(testPage.title) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickRemoveFromShortcutsButton(composeTestRule) {
            verifySnackBarText(getStringResource(R.string.snackbar_top_site_removed))
        }.goToHomescreenWithRedesignedToolbar {
            verifyNotExistingTopSitesList(testPage.title)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776969
    @SmokeTest
    @Test
    fun verifyTheAddToHomeScreenSubMenuOptionTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickAddToHomeScreenButton(composeTestRule) {
            clickCancelShortcutButton()
        }
        browserScreen {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickAddToHomeScreenButton(composeTestRule) {
            clickAddShortcutButton()
            clickSystemHomeScreenShortcutAddButton()
        }.openHomeScreenShortcut(testPage.title) {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776970
    @SmokeTest
    @Test
    fun verifyTheSaveToCollectionSubMenuOptionTest() {
        val collectionTitle = "First Collection"
        val firstTestPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondTestPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstTestPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickSaveToCollectionButton(composeTestRule) {
        }.typeCollectionNameAndSave(collectionTitle) {
            verifySnackBarText("Collection saved!")
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondTestPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickSaveToCollectionButton(composeTestRule) {
        }.selectExistingCollection(collectionTitle) {
            verifySnackBarText("Tab saved!")
            clickSnackbarButton(composeTestRule, "VIEW")
        }
        homeScreen {
        }.expandCollection(collectionTitle) {
            verifyTabSavedInCollection(firstTestPage.title)
            verifyTabSavedInCollection(secondTestPage.title)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776971
    @SmokeTest
    @Test
    fun verifyTheSaveAsPDFSubMenuOptionTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            clickSaveButton(composeTestRule)
        }.clickSaveAsPDFButton(composeTestRule) {
            verifyDownloadPrompt(testPage.title + ".pdf")
        }.clickDownload {
        }.clickOpen("application/pdf") {
            assertAppWithPackageNameOpens(packageName)
            verifyUrl("content://media/external_primary/downloads/")
            verifyTabCounter("2")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776951
    @SmokeTest
    @Test
    fun verifyTheDefaultToolsMenuItemsTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            openToolsMenu(composeTestRule)
            verifyTheDefaultToolsMenuItems(composeTestRule)
            verifyReaderViewButtonIsEnabled(composeTestRule, isEnabled = false)
            verifyOpenInAppButtonIsEnabled(composeTestRule, isEnabled = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776961
    @SmokeTest
    @Test
    fun verifyTheReaderViewButtonTest() {
        val readerViewPage = TestAssetHelper.getLoremIpsumAsset(mockWebServer)
        val estimatedReadingTime = "1 - 2 minutes"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(readerViewPage.url) {
            verifyPageContent(readerViewPage.content)
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            openToolsMenu(composeTestRule)
            verifyReaderViewButtonIsEnabled(composeTestRule, isEnabled = true)
        }.clickTheReaderViewModeButton(composeTestRule) {
            verifyPageContent(estimatedReadingTime)
            composeTestRule.waitForIdle()
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            openToolsMenu(composeTestRule)
            verifyCustomizeReaderViewButtonIsDisplayed(composeTestRule, isDisplayed = true)
        }.clickCustomizeReaderViewButton(composeTestRule) {
            verifyAppearanceFontGroup(true)
            verifyAppearanceFontSansSerif(true)
            verifyAppearanceFontSerif(true)
            verifyAppearanceFontIncrease(true)
            verifyAppearanceFontDecrease(true)
            verifyAppearanceFontSize(3)
            verifyAppearanceColorGroup(true)
            verifyAppearanceColorDark(true)
            verifyAppearanceColorLight(true)
            verifyAppearanceColorSepia(true)
        }.closeAppearanceMenu {
        }.openThreeDotMenuFromRedesignedToolbar {
            openToolsMenu(composeTestRule)
        }.clickTurnOffReaderViewButton(composeTestRule) {
        }.openThreeDotMenuFromRedesignedToolbar {
            openToolsMenu(composeTestRule)
            verifyReaderViewButtonIsEnabled(composeTestRule, isEnabled = true)
            verifyCustomizeReaderViewButtonIsDisplayed(composeTestRule, isDisplayed = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776953
    @SmokeTest
    @Test
    fun verifyTheTranslatePageButtonsStatesTest() {
        val testPage = TestAssetHelper.getForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            openToolsMenu(composeTestRule)
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(composeTestRule, isDisplayed = true)
        }.clickTranslateButton(composeTestRule) {
        }.openThreeDotMenuFromRedesignedToolbar {
            openToolsMenu(composeTestRule)
        }.clickTranslatedToButton(composeTestRule, "English") {
            verifyTranslationSheetIsDisplayed(composeTestRule, isDisplayed = true)
        }.clickShowOriginalButton(composeTestRule) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenuFromRedesignedToolbar {
            openToolsMenu(composeTestRule)
            verifyTheDefaultToolsMenuItems(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776955
    @SmokeTest
    @Test
    fun verifyTheShareButtonTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            openToolsMenu(composeTestRule)
        }.clickShareButton(composeTestRule) {
            verifyShareTabLayout()
            verifySharingWithSelectedApp(
                appName = "Gmail",
                content = testPage.url.toString(),
                subject = testPage.title,
            )
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2776956
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1936933")
    @SmokeTest
    @Test
    fun verifyOpenInAppButtonIsEnabledTest() {
        val youtubeURL = "vnd.youtube://".toUri()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(youtubeURL) {
            waitForPageToLoad(waitingTime)
        }.openThreeDotMenuFromRedesignedToolbar {
            expandRedesignedMenu(composeTestRule)
            openToolsMenu(composeTestRule)
            verifyOpenInAppButtonIsEnabled(composeTestRule, appName = "YouTube", isEnabled = true)
            clickOpenInAppButton(composeTestRule, appName = "YouTube")
            assertYoutubeAppOpens()
        }
    }
}
