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
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertExternalAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertYoutubeAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.clickSystemHomeScreenShortcutAddButton
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_DOCS
import org.mozilla.fenix.helpers.Constants.PackageName.PRINT_SPOOLER
import org.mozilla.fenix.helpers.DataGenerationHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.DataGenerationHelper.getRecommendedExtensionTitle
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MockBrowserDataHelper
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeVeryShort
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.closeApp
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestHelper.waitUntilSnackbarGone
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.helpers.perf.DetectMemoryLeaksRule
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.mainMenuScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class MainMenuTestCompose : TestSetup() {
    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                skipOnboarding = true,
                isMenuRedesignEnabled = true,
                isMenuRedesignCFREnabled = false,
                isPageLoadTranslationsPromptEnabled = false,
            ),
        ) { it.activity }

    @get:Rule
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java,
        true,
        false,
    )

    @get:Rule
    val memoryLeaksRule = DetectMemoryLeaksRule()

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860735
    @SmokeTest
    @Test
    fun homepageRedesignedMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
            verifyHomeMainMenuItems()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860835
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun webpageRedesignedMenuItemsTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            verifyPageMainMenuItems()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860844
    @SmokeTest
    @Test
    fun verifySwitchToDesktopSiteIsDisabledOnPDFsTest() {
        val pdfPage = TestAssetHelper.getPdfFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pdfPage.url) {
            waitForPageToLoad()
            verifyPageContent(pdfPage.content)
        }.openThreeDotMenu(composeTestRule) {
            verifySwitchToDesktopSiteButtonIsEnabled(isEnabled = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860846
    @SmokeTest
    @Test
    fun findInPageTest() {
        val testPage = getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu(composeTestRule) {
        }.clickFindInPageButton {
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
        }.openThreeDotMenu(composeTestRule) {
        }.clickFindInPageButton {
            enterFindInPageQuery("3")
            verifyFindInPageResult("1/1")
        }.closeFindInPageWithBackButton {
            verifyFindInPageBar(false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860850
    @SmokeTest
    @Test
    fun verifyBookmarksMenuButtonTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openBookmarks(composeTestRule) {
            verifyEmptyBookmarksMenuView()
        }.goBackToBrowserScreen {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860851
    @SmokeTest
    @Test
    fun verifyHistoryMenuButtonTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openHistory {
            verifyHistoryMenuView()
        }.goBack {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860852
    @SmokeTest
    @Test
    fun verifyDownloadsMenuButtonTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openDownloads {
            verifyEmptyDownloadsList(composeTestRule)
            exitMenu()
        }.exitDownloadsManagerToBrowser(composeTestRule) {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860853
    @SmokeTest
    @Test
    fun verifyPasswordsMenuButtonTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openPasswords {
            verifySecurityPromptForLogins()
            tapSetupLater()
            verifyEmptySavedLoginsListView()
            TestHelper.exitMenu()
        }
        browserScreen {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860757
    // Verifies the main menu of a custom tab with a custom menu item
    @SmokeTest
    @Test
    fun verifyCustomTabMenuItemsTest() {
        val customMenuItem = "TestMenuItem"
        val customTabPage = getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenuFromRedesignedToolbar {
            verifyRedesignedCustomTabsMainMenuItemsExist(customMenuItem, true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860761
    // The test opens a link in a custom tab then sends it to the browser
    @SmokeTest
    @Test
    fun openCustomTabInFirefoxTest() {
        val customTabPage = getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenuFromRedesignedToolbar {
        }.clickOpenInBrowserButtonFromRedesignedToolbar(composeTestRule) {
            verifyPageContent(customTabPage.content)
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860771
    @SmokeTest
    @Test
    fun verifyRecommendedExtensionsListTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            verifyRecommendedAddonsViewFromRedesignedMainMenu(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860779
    @SmokeTest
    @Test
    fun verifyRedesignedMenuAfterRemovingAnExtensionTest() {
        var recommendedExtensionTitle = ""
        val genericURL = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            recommendedExtensionTitle = getRecommendedExtensionTitle(composeTestRule)
            installRecommendedAddon(recommendedExtensionTitle, composeTestRule)
            verifyAddonPermissionPrompt(recommendedExtensionTitle)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompletedPrompt(recommendedExtensionTitle, composeTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            clickManageExtensionsButtonFromRedesignedMainMenu(composeTestRule)
        }.openDetailedMenuForAddon(recommendedExtensionTitle) {
        }.removeAddon(composeTestRule.activityRule) {
            verifySnackBarText("Successfully uninstalled $recommendedExtensionTitle")
            waitUntilSnackbarGone()
        }.goBack {
        }
        browserScreen {
        }.openThreeDotMenu(composeTestRule) {
            verifyTryRecommendedExtensionButton()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860784
    @SmokeTest
    @Test
    fun verifyTheManageExtensionsSubMenuTest() {
        var recommendedExtensionTitle = ""
        val genericURL = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            recommendedExtensionTitle = getRecommendedExtensionTitle(composeTestRule)
            installRecommendedAddon(recommendedExtensionTitle, composeTestRule)
            verifyAddonPermissionPrompt(recommendedExtensionTitle)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompletedPrompt(recommendedExtensionTitle, composeTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            clickManageExtensionsButtonFromRedesignedMainMenu(composeTestRule)
            verifyAddonIsInstalled(recommendedExtensionTitle)
            verifyEnabledTitleDisplayed()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860813
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1968653")
    @SmokeTest
    @Test
    fun verifyTheBookmarkThisPageSubMenuItemsTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickBookmarkThisPageButton {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickEditBookmarkButton(composeTestRule) {
            verifyEditBookmarksView()
            clickDeleteBookmarkButtonInEditMode()
        }
        browserScreen {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
            verifyBookmarkThisPageButton()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860814
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun verifyTheAddToShortcutsSubMenuOptionTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickAddToShortcutsButton {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreen(composeTestRule) {
            verifyExistingTopSitesTabs(composeTestRule, testPage.title)
        }.openTopSiteTabWithTitle(composeTestRule, testPage.title) {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickRemoveFromShortcutsButton {
            verifySnackBarText(getStringResource(R.string.snackbar_top_site_removed))
        }.goToHomescreen(composeTestRule) {
            verifyNotExistingTopSiteItem(composeTestRule, testPage.title)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860815
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun verifyTheAddToHomeScreenSubMenuOptionTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickAddToHomeScreenButton {
            clickCancelShortcutButton()
        }
        browserScreen {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickAddToHomeScreenButton {
            clickAddShortcutButton()
            clickSystemHomeScreenShortcutAddButton()
        }.openHomeScreenShortcut(testPage.title) {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860816
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun verifyTheSaveToCollectionSubMenuOptionTest() {
        val collectionTitle = "First Collection"
        val firstTestPage = getGenericAsset(mockWebServer, 1)
        val secondTestPage = getGenericAsset(mockWebServer, 2)

        composeTestRule.activityRule.applySettingsExceptions {
            // Disabling these features to have better visibility of the Collections view
            it.isRecentlyVisitedFeatureEnabled = false
            it.isRecentTabsFeatureEnabled = false
        }

        MockBrowserDataHelper
            .createCollection(
                Pair(firstTestPage.url.toString(), firstTestPage.title),
                title = collectionTitle,
            )

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondTestPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickSaveToCollectionButton {
        }.selectExistingCollection(collectionTitle) {
            verifySnackBarText("Tab saved!")
        }.goToHomescreen(composeTestRule) {
        }.expandCollection(composeTestRule, collectionTitle) {
            verifyTabSavedInCollection(composeTestRule, firstTestPage.title)
            verifyTabSavedInCollection(composeTestRule, secondTestPage.title)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860817
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1968653")
    @SmokeTest
    @Test
    fun verifyTheSaveAsPDFSubMenuOptionTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            clickSaveButton()
        }.clickSaveAsPDFButton {
            verifyDownloadPrompt(testPage.title + ".pdf")
        }.clickDownload {
        }.clickOpen("application/pdf") {
            assertExternalAppOpens(GOOGLE_DOCS)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860799
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun verifyTheTranslatePageButtonsStatesTest() {
        val testPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURL(testPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.clickTranslateButton {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickTranslateButton {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.clickTranslatedToButton("English") {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickShowOriginalButton {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860802
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun verifyTheShareButtonTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.clickShareButton {
            verifyShareTabLayout()
            verifySharingWithSelectedApp(
                appName = "Gmail",
                content = testPage.url.toString(),
                subject = testPage.title,
            )
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860804
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun verifyOpenInAppButtonIsEnabledTest() {
        val youtubeURL = "vnd.youtube://".toUri()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(youtubeURL) {
            waitForPageToLoad(waitingTime)
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
            verifyOpenInAppButtonIsEnabled(appName = "YouTube", isEnabled = true)
            clickOpenInAppButton(appName = "YouTube")
            assertYoutubeAppOpens()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860845
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1968653")
    @SmokeTest
    @Test
    fun switchDesktopSiteModeOnOffTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            verifySwitchToDesktopSiteButton()
            clickSwitchToDesktopSiteButton()
        }
        browserScreen {
            waitForPageToLoad(pageLoadWaitingTime = waitingTimeLong)
        }.openThreeDotMenu(composeTestRule) {
            verifySwitchToMobileSiteButton()
            clickSwitchToMobileSiteButton()
        }
        browserScreen {
            waitForPageToLoad(pageLoadWaitingTime = waitingTimeLong)
        }.openThreeDotMenu(composeTestRule) {
            verifySwitchToDesktopSiteButton()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860801
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @SmokeTest
    @Test
    fun verifyPrintSubMenuOptionTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
            clickPrintContentButton()
            assertNativeAppOpens(PRINT_SPOOLER)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860775
    @SmokeTest
    @Test
    fun verifyExtensionInstallTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            val recommendedExtensionTitle = getRecommendedExtensionTitle(composeTestRule)
            installRecommendedAddon(recommendedExtensionTitle, composeTestRule)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompletedPrompt(recommendedExtensionTitle, composeTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860741
    @Test
    fun verifyTheHomePageMainMenuSettingsButtonTest() {
        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
        }.openSettings {
            verifySettingsToolbar()
        }.goBack {
            verifyHomeWordmark()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860751
    @Test
    fun verifyTheHomePageMainMenuCustomizeHomepageButtonTest() {
        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
        }.clickCustomizeHomepageButton {
            verifyHomePageView()
        }.goBackToHomeScreen {
            verifyHomeWordmark()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860725
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971959")
    @Test
    fun verifyTheHomePageMainMenuCFRTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isMenuRedesignCFREnabled = true
        }
        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
            verifyMainMenuCFR()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2728842
    @Test
    fun verifyFindInPageInPDFTest() {
        val testPage = getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            clickPageObject(MatcherHelper.itemWithText("PDF form file"))
            clickPageObject(itemWithResIdAndText("android:id/button2", "CANCEL"))
        }.openThreeDotMenu(composeTestRule) {
        }.clickFindInPageButton {
            verifyFindInPageNextButton()
            verifyFindInPagePrevButton()
            verifyFindInPageCloseButton()
            enterFindInPageQuery("l")
            verifyFindInPageResult("1/2")
            clickFindInPageNextButton()
            verifyFindInPageResult("2/2")
            clickFindInPagePrevButton()
            verifyFindInPageResult("1/2")
        }.closeFindInPageWithCloseButton {
            verifyFindInPageBar(false)
        }.openThreeDotMenu(composeTestRule) {
        }.clickFindInPageButton {
            enterFindInPageQuery("p")
            verifyFindInPageResult("1/1")
        }.closeFindInPageWithBackButton {
            verifyFindInPageBar(false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860854
    @Test
    fun verifyTheQuitFirefoxMenuItemTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            verifyDeleteBrowsingOnQuitEnabled(false)
            clickDeleteBrowsingOnQuitButtonSwitch()
            verifyDeleteBrowsingOnQuitEnabled(true)
        }.goBack {
            verifySettingsOptionSummary("Delete browsing data on quit", "On")
        }.goBack {
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
            clickQuitFirefoxButton()
            restartApp(composeTestRule.activityRule)
        }
        homeScreen {
            verifyHomeWordmark()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860759
    @Test
    fun verifyEnabledDesktopSiteToggleInCustomTabTest() {
        val customTabPage = getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
        }.openMainMenuFromRedesignedToolbar {
            verifySwitchToDesktopSiteButton(composeTestRule)
            clickSwitchToDesktopSiteButton(composeTestRule)
        }.openMainMenuFromRedesignedToolbar {
            verifySwitchToMobileSiteButton(composeTestRule)
            clickSwitchToMobileSiteButton(composeTestRule)
        }.openMainMenuFromRedesignedToolbar {
            verifySwitchToDesktopSiteButton(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860760
    @Test
    fun customTabsFindInPageTest() {
        val customTabPage = getGenericAsset(mockWebServer, 3)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
        }.openMainMenuFromRedesignedToolbar {
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
        }.openThreeDotMenu(composeTestRule) {
        }.clickFindInPageButton {
            enterFindInPageQuery("3")
            verifyFindInPageResult("1/1")
        }.closeFindInPageWithBackButton {
            verifyFindInPageBar(false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860765
    @Test
    fun verifyTheDismissalWhenTappingOutsideTheCustomTabMainMenuTest() {
        val customMenuItem = "TestMenuItem"
        val customTabPage = getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
        }.openMainMenuFromRedesignedToolbar {
        }.clickOutsideTheMainMenu {
        }
        customTabScreen {
            verifyRedesignedCustomTabsMainMenuItemsExist(customMenuItem, false, waitingTimeVeryShort)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860770
    @Test
    fun noInstalledExtensionsTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
            verifyTryRecommendedExtensionButton()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860781
    @Test
    fun disabledExtensionTest() {
        var recommendedExtensionTitle = ""
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            recommendedExtensionTitle = getRecommendedExtensionTitle(composeTestRule)
            installRecommendedAddon(recommendedExtensionTitle, composeTestRule)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompletedPrompt(recommendedExtensionTitle, composeTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }
        browserScreen {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            clickManageExtensionsButtonFromRedesignedMainMenu(composeTestRule)
        }.openDetailedMenuForAddon(recommendedExtensionTitle) {
            disableExtension()
            waitUntilSnackbarGone()
        }.goBack {
        }.goBackToBrowser {
        }.openThreeDotMenu(composeTestRule) {
            verifyNoExtensionsEnabledButton()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860785
    @Test
    fun verifyTheDiscoverMoreExtensionSubMenuItemTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
        }.clickDiscoverMoreExtensionsButton(composeTestRule) {
            verifyUrl("addons.mozilla.org/en-US/android")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860790
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971958")
    @Test
    fun verifyTheClosingBehaviourWhenTappingOutsideTheExtensionsSubMenuTest() {
        var recommendedExtensionTitle = ""
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
        }
        mainMenuScreen(composeTestRule) {
        }.clickOutsideTheMainMenu {
            verifyExtensionsMenuDoesNotExist()
        }.openThreeDotMenu(composeTestRule) {
        }.openExtensionsFromMainMenu {
            recommendedExtensionTitle = getRecommendedExtensionTitle(composeTestRule)
            installRecommendedAddon(recommendedExtensionTitle, composeTestRule)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompletedPrompt(recommendedExtensionTitle, composeTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }
        mainMenuScreen(composeTestRule) {
        }.clickOutsideTheMainMenu {
            verifyExtensionsMenuDoesNotExist()
        }
        // Steps not applicable anymore due to recent main menu redesign changes
        // Will revise when the final implementation is done
        // Tracking ticket: https://bugzilla.mozilla.org/show_bug.cgi?id=1971939

        // browserScreen {
        // }.openThreeDotMenu(composeTestRule) {
        // }.openExtensionsFromMainMenu {
        //     clickManageExtensionsButtonFromRedesignedMainMenu(composeTestRule)
        // }.openDetailedMenuForAddon(recommendedExtensionTitle) {
        //     disableExtension()
        //     waitUntilSnackbarGone()
        // }.goBack {
        // }.goBackToBrowser {
        // }.openThreeDotMenu(composeTestRule) {
        // }.openExtensionsFromMainMenu {
        //     verifyManageExtensionsButtonFromRedesignedMainMenu(composeTestRule)
        // }
        // mainMenuScreen(composeTestRule) {
        // }.clickOutsideTheMainMenu {
        //     verifyExtensionsMenuDoesNotExist()
        // }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2860800
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @Test
    fun verifyTheReportBrokenSiteOptionTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.clickReportBrokenSiteButton {
            verifyWebCompatReporterViewItems(composeTestRule, websiteURL = defaultWebPage.url.toString())
        }.closeWebCompatReporter {
        }.openThreeDotMenu(composeTestRule) {
        }.openSettings {
        }.openSettingsSubMenuDataCollection {
            clickUsageAndTechnicalDataToggle()
            verifyUsageAndTechnicalDataToggle(enabled = false)
        }

        exitMenu()

        browserScreen {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.clickReportBrokenSiteButton {
            verifyUrl("webcompat.com/issues/new")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2939173
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @Test
    fun verifyTheWhatIsBrokenErrorMessageTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWebCompatReporterViewItems(composeTestRule, defaultWebPage.url.toString())
            verifyWhatIsBrokenField(composeTestRule)
            verifySendButtonIsEnabled(composeTestRule, isEnabled = false)
            clickChooseReasonField(composeTestRule)
            clickSiteDoesNotLoadReason(composeTestRule)
            verifyChooseReasonErrorMessageIsNotDisplayed(composeTestRule)
            verifySendButtonIsEnabled(composeTestRule, isEnabled = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2939175
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @Test
    fun verifyThatTheBrokenSiteFormCanBeCanceledTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWebCompatReporterViewItems(composeTestRule, defaultWebPage.url.toString())
            clickChooseReasonField(composeTestRule)
            clickSiteDoesNotLoadReason(composeTestRule)
            clickBrokenSiteFormCancelButton(composeTestRule)
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWhatIsBrokenField(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2939176
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1968653")
    @Test
    fun verifyTheBrokenSiteFormSubmissionTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWebCompatReporterViewItems(composeTestRule, defaultWebPage.url.toString())
            clickChooseReasonField(composeTestRule)
            clickSiteDoesNotLoadReason(composeTestRule)
            describeBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time")
            clickBrokenSiteFormSendButton(composeTestRule)
        }
        browserScreen {
            verifySnackBarText("Your report was sent")
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWhatIsBrokenField(composeTestRule)
            verifyBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time", isDisplayed = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2939179
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @Test
    fun verifyThatTheBrokenSiteFormInfoPersistsTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWebCompatReporterViewItems(composeTestRule, defaultWebPage.url.toString())
            clickChooseReasonField(composeTestRule)
            clickSiteDoesNotLoadReason(composeTestRule)
            describeBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time")
        }.closeWebCompatReporter {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time", isDisplayed = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2939180
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @Test
    fun verifyTheBrokenSiteFormIsEmptyWithoutSubmittingThePreviousOneTest() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWebCompatReporterViewItems(composeTestRule, firstWebPage.url.toString())
            clickChooseReasonField(composeTestRule)
            clickSiteDoesNotLoadReason(composeTestRule)
            describeBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time")
        }.closeWebCompatReporter {
        }.openTabDrawer(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWhatIsBrokenField(composeTestRule)
            verifyBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time", isDisplayed = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2939181
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @Test
    fun verifyThatTheBrokenSiteFormInfoIsErasedWhenKillingTheAppTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWebCompatReporterViewItems(composeTestRule, defaultWebPage.url.toString())
            clickChooseReasonField(composeTestRule)
            clickSiteDoesNotLoadReason(composeTestRule)
            describeBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time")
        }
        closeApp(composeTestRule.activityRule)
        restartApp(composeTestRule.activityRule)

        browserScreen {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyWhatIsBrokenField(composeTestRule)
            verifyBrokenSiteProblem(composeTestRule, problemDescription = "Prolonged page loading time", isDisplayed = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2939182
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1971476")
    @Test
    fun verifyReportBrokenSiteFormNotDisplayedWhenTelemetryIsDisabledTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu(composeTestRule) {
        }.openSettings {
        }.openSettingsSubMenuDataCollection {
            clickUsageAndTechnicalDataToggle()
            verifyUsageAndTechnicalDataToggle(enabled = false)
        }
        exitMenu()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu(composeTestRule) {
            openToolsMenu()
        }.openReportBrokenSite {
            verifyUrl("webcompat.com/issues/new")
        }
    }
}
