/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SkipLeaks
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.disableWifiNetworkConnection
import org.mozilla.fenix.helpers.AppAndSystemHelper.enableDataSaverSystemSetting
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.helpers.perf.DetectMemoryLeaksRule
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.translationsRobot

class TranslationsTest : TestSetup() {
    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                skipOnboarding = true,
                isMenuRedesignEnabled = false,
                isMenuRedesignCFREnabled = false,
                isPageLoadTranslationsPromptEnabled = true,
            ),
        ) { it.activity }

    @get:Rule
    val memoryLeaksRule = DetectMemoryLeaksRule()

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2436643
    @SmokeTest
    @Test
    @SkipLeaks
    fun verifyTheFirstTranslationNotNowButtonFunctionalityTest() {
        val testPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickNotNowButton {
        }
        navigationToolbar {
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.swipeCloseTranslationsSheet {
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2437107
    @SmokeTest
    @Test
    fun verifyMainMenuTranslationButtonFunctionalityTest() {
        val testPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickNotNowButton {
        }
        navigationToolbar {
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickTranslateButton {
            verifyPageContent("Article of the day")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2439667
    @SmokeTest
    @Test
    fun verifyTheDownloadLanguagesFunctionalityTest() {
        val firstTestPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstTestPage.url) {
            verifyPageContent(firstTestPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            clickTranslationsOptionsButton()
        }.clickTranslationSettingsButton {
            clickDownloadLanguagesButton()
            clickLanguageToDownload("Bosnian")
            verifyDownloadedLanguage("Bosnian")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2437991
    @SmokeTest
    @Test
    fun verifyTheNeverTranslateOptionTest() {
        val firstTestPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)
        val secondTestPage = TestAssetHelper.getSecondForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstTestPage.url) {
            verifyPageContent(firstTestPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            clickTranslationsOptionsButton()
            clickNeverTranslateLanguageOption(languageToTranslate = "French")
            verifyTheNeverTranslateLanguageDescription()
            verifyTheNeverTranslateLanguageOptionIsChecked(isChecked = true)
        }.clickTranslationSettingsButton {
            clickAutomaticTranslationButton()
            verifyNeverAutomaticallyTranslateForLanguage(languageToTranslate = "French")
            clickLanguageFromAutomaticTranslationMenu(languageToTranslate = "French")
            verifyNeverTranslateOptionState(isChecked = true)
        }.goBackToAutomaticTranslationSubMenu {
        }.goBackToTranslationSettingsSubMenu {
        }.goBackToTranslationOptionSheet {
            closeTranslationsSheet()
        }
        browserScreen {
        }.openTabDrawer(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondTestPage.url.toString()) {
            waitForPageToLoad(pageLoadWaitingTime = waitingTimeLong)
            verifyPageContent(secondTestPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2436642
    @Test
    fun verifyFirstTranslationBottomSheetTranslateFunctionalityTest() {
        val testPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickTranslateButton {
            verifyPageContent("Article of the day")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2437112
    @Test
    fun verifyTheShowOriginalTranslationOptionTest() {
        val testPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickTranslateButton {
            verifyPageContent("Article of the day")
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickShowOriginalButton {
            verifyPageContent(testPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2437111
    @Test
    fun changeTheTranslateToLanguageTest() {
        val testPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            verifyPageContent(testPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickTranslateButton {
            verifyPageContent("Article of the day")
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            clickTranslateToDropdown()
            clickTranslateToLanguage("Estonian")
        }.clickTranslateButton(pageWasNotPreviouslyTranslated = false) {
            verifyPageContent("Päeva artikkel")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2437992
    @Test
    fun verifyTheAlwaysTranslateOptionTest() {
        val firstTestPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)
        val secondTestPage = TestAssetHelper.getSecondForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondTestPage.url) {
            verifyPageContent(secondTestPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            clickTranslationsOptionsButton()
            verifyAlwaysTranslateOptionIsChecked(isChecked = false)
            clickAlwaysTranslateLanguageOption("French")
            verifyTheAlwaysTranslateLanguageDescription()
        }.clickTranslationSettingsButton {
            clickAutomaticTranslationButton()
            verifyAlwaysAutomaticallyTranslateForLanguage(languageToTranslate = "French")
            clickLanguageFromAutomaticTranslationMenu("French")
            verifyAlwaysTranslateOptionState(isChecked = true)
        }.goBackToAutomaticTranslationSubMenu {
        }.goBackToTranslationSettingsSubMenu {
        }.goBackToTranslationOptionSheet {
            closeTranslationsSheet()
        }
        browserScreen {
            waitForPageToLoad(pageLoadWaitingTime = waitingTimeLong)
            verifyPageContent("Word of the day")
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstTestPage.url) {
            verifyPageContent("Article of the day")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2439960
    @Test
    fun verifyTheSiteDeletionFromTheNeverTranslateListTest() {
        val firstTestPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstTestPage.url) {
            verifyPageContent(firstTestPage.content)
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            clickTranslationsOptionsButton()
            verifyTheNeverTranslateThisSiteOptionIsChecked(isChecked = false)
            clickNeverTranslateThisSiteOption()
            verifyTheNeverTranslateThisSiteOptionIsChecked(isChecked = true)
            verifyAlwaysOfferToTranslateOptionIsEnabled(isEnabled = false)
            verifyAlwaysTranslateOptionIsEnabled(isEnabled = false)
            verifyTheNeverTranslateLanguageOptionIsEnabled(isEnabled = false)
        }.clickTranslationSettingsButton {
            clickNeverTranslateTheseSitesButton()
            verifyNeverTranslateThisSiteRemoveButton("${firstTestPage.url.scheme}://${firstTestPage.url.authority}")
            clickNeverTranslateThisSiteRemoveButton("${firstTestPage.url.scheme}://${firstTestPage.url.authority}")
            verifyDeleteNeverTranslateThisSiteDialog("${firstTestPage.url.scheme}://${firstTestPage.url.authority}")
            clickCancelDeleteNeverTranslateThisSiteDialog()
            clickNeverTranslateThisSiteRemoveButton("${firstTestPage.url.scheme}://${firstTestPage.url.authority}")
            verifyDeleteNeverTranslateThisSiteDialog("${firstTestPage.url.scheme}://${firstTestPage.url.authority}")
            clickConfirmDeleteNeverTranslateThisSiteDialog()
        }.goBackToTranslationSettingsSubMenu {
        }.goBackToTranslationOptionSheet {
            verifyTheNeverTranslateThisSiteOptionIsChecked(isChecked = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2440963
    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1965222")
    @Test
    fun downloadLanguageWhileDataSaverModeIsOnTest() {
        val firstTestPage = TestAssetHelper.getFirstForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURL(firstTestPage.url) {
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            clickTranslationsOptionsButton()
        }.clickTranslationSettingsButton {
            disableWifiNetworkConnection()
            enableDataSaverSystemSetting(enabled = true)
            clickDownloadLanguagesButton()
            clickLanguageToDownload("Bosnian")
            verifyDownloadLanguageInSavingModePrompt()
            clickCancelDownloadLanguageInSavingModePromptButton()
            clickLanguageToDownload("Bosnian")
            verifyDownloadLanguageInSavingModePrompt()
            clickDownloadLanguageInSavingModePromptButton()
            verifyDownloadedLanguage("Bosnian")
        }
    }
}
