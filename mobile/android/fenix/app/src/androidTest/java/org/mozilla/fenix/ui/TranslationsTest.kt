/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.translationsRobot

class TranslationsTest : TestSetup() {
    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                skipOnboarding = true,
                isNavigationToolbarEnabled = false,
                isNavigationBarCFREnabled = false,
                isSetAsDefaultBrowserPromptEnabled = false,
                isMenuRedesignEnabled = false,
                isMenuRedesignCFREnabled = false,
                isPageLoadTranslationsPromptEnabled = true,
            ),
        ) { it.activity }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2436643
    @SmokeTest
    @Test
    fun verifyTheFirstTranslationNotNowButtonFunctionalityTest() {
        val testPage = TestAssetHelper.getForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURL(testPage.url) {
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickNotNowButton {
        }
        navigationToolbar {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.swipeCloseTranslationsSheet {
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2437105
    @SmokeTest
    @Test
    fun verifyTranslationFunctionalityUsingToolbarButtonTest() {
        val testPage = TestAssetHelper.getForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURL(testPage.url) {
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickNotNowButton {
        }
        navigationToolbar {
        }.clickTranslateButton(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickTranslateButton {
        }
        navigationToolbar {
            verifyTranslationButton(
                isPageTranslated = true,
                originalLanguage = "French",
                translatedLanguage = "English",
            )
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2437107
    @SmokeTest
    @Test
    fun verifyMainMenuTranslationButtonFunctionalityTest() {
        val testPage = TestAssetHelper.getForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURL(testPage.url) {
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickNotNowButton {
        }
        navigationToolbar {
        }.openThreeDotMenu {
        }.clickTranslateButton(composeTestRule) {
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }.clickTranslateButton {
        }

        navigationToolbar {
            verifyTranslationButton(
                isPageTranslated = true,
                originalLanguage = "French",
                translatedLanguage = "English",
            )
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2443276
    @SmokeTest
    @Test
    fun verifyTheTranslationIsDisplayedAutomaticallyTest() {
        val firstTestPage = TestAssetHelper.getForeignWebPageAsset(mockWebServer)
        val secondTestPage = "https://support.mozilla.org/de/"

        navigationToolbar {
        }.enterURL(firstTestPage.url) {
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            closeTranslationsSheet()
        }
        browserScreen {
        }.openTabDrawer(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondTestPage) {
            waitForPageToLoad(pageLoadWaitingTime = waitingTimeLong)
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2439667
    @SmokeTest
    @Test
    fun verifyTheDownloadLanguagesFunctionalityTest() {
        val firstTestPage = TestAssetHelper.getForeignWebPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURL(firstTestPage.url) {
        }
        translationsRobot(composeTestRule) {
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
        val firstTestPage = TestAssetHelper.getForeignWebPageAsset(mockWebServer)
        val secondTestPage = "https://support.mozilla.org/fr/"

        navigationToolbar {
        }.enterURL(firstTestPage.url) {
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = true)
            clickTranslationsOptionsButton()
            clickNeverTranslateLanguageOption(languageToTranslate = "French")
            verifyTheNeverTranslateLanguageDescription()
            verifyTheNeverTranslateLanguageOptionState(isChecked = true)
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
        }.submitQuery(secondTestPage) {
            waitForPageToLoad(pageLoadWaitingTime = waitingTimeLong)
        }
        translationsRobot(composeTestRule) {
            verifyTranslationSheetIsDisplayed(isDisplayed = false)
        }
        navigationToolbar {
            verifyTranslationButton(isPageTranslated = false)
        }
    }
}
