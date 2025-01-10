/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.assert
import androidx.compose.ui.test.hasAnySibling
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onAllNodesWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemIsChecked
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.TestHelper.mDevice

class SettingsTranslationsRobot(private val composeTestRule: ComposeTestRule) {

    fun clickDownloadLanguagesButton() {
        Log.i(TAG, "clickDownloadLanguagesButton: Trying to click the \"Download languages\" button")
        composeTestRule.onNodeWithText(getStringResource(R.string.translation_settings_download_language)).performClick()
        Log.i(TAG, "clickDownloadLanguagesButton: Clicked the \"Download languages\" button")
    }

    fun clickLanguageToDownload(languageToDownload: String) {
        Log.i(TAG, "clickLanguageToDownload: Trying to click the $languageToDownload language download button")
        composeTestRule.onAllNodesWithContentDescription("$languageToDownload Download", substring = true)
        Log.i(TAG, "clickLanguageToDownload: Clicked the $languageToDownload language download button")
    }

    fun verifyDownloadedLanguage(downloadedLanguage: String) {
        Log.i(TAG, "verifyDownloadedLanguage: Trying to verify that $downloadedLanguage language is downloaded")
        composeTestRule.onAllNodesWithContentDescription("$downloadedLanguage MBDelete", substring = true)
        Log.i(TAG, "verifyDownloadedLanguage: Verified that $downloadedLanguage language is downloaded")
    }

    fun clickAutomaticTranslationButton() {
        Log.i(TAG, "clickAutomaticTranslationButton: Trying to click the \"Automatic translation\" button")
        composeTestRule.onNodeWithText(getStringResource(R.string.translation_settings_automatic_translation)).performClick()
        Log.i(TAG, "clickAutomaticTranslationButton: Clicked the \"Automatic translation\" button")
    }

    fun verifyNeverAutomaticallyTranslateForLanguage(languageToTranslate: String) {
        Log.i(TAG, "verifyNeverAutomaticallyTranslateForLanguage: Trying to verify that $languageToTranslate language is set to \"Never translate\"")
        composeTestRule.onNodeWithText(languageToTranslate, useUnmergedTree = true)
            .assert(hasAnySibling(hasText(getStringResource(R.string.automatic_translation_option_never_translate_title_preference))))
        Log.i(TAG, "verifyNeverAutomaticallyTranslateForLanguage: Verified that $languageToTranslate language is set to \"Never translate\"")
    }

    fun clickLanguageFromAutomaticTranslationMenu(languageToTranslate: String) {
        Log.i(TAG, "clickLanguageFromAutomaticTranslationMenu: Trying to click $languageToTranslate language from the \"Automatic translation\" menu")
        composeTestRule.onNodeWithText(languageToTranslate).performClick()
        Log.i(TAG, "clickLanguageFromAutomaticTranslationMenu: Clicked $languageToTranslate language from the \"Automatic translation\" menu")
    }

    fun verifyNeverTranslateOptionState(isChecked: Boolean) =
        assertItemIsChecked(
            mDevice.findObject(
                UiSelector()
                    .index(2)
                    .className("android.view.View"),
            ),
            isChecked = isChecked,
        )

    class Transition(private val composeTestRule: ComposeTestRule) {
        fun goBackToAutomaticTranslationSubMenu(interact: SettingsTranslationsRobot.() -> Unit): Transition {
            Log.i(TAG, "goBackToAutomaticTranslationSubMenu: Trying to click the navigate up button")
            itemWithDescription(getStringResource(R.string.action_bar_up_description)).click()
            Log.i(TAG, "goBackToAutomaticTranslationSubMenu: Clicked the navigate up button")

            SettingsTranslationsRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun goBackToTranslationSettingsSubMenu(interact: SettingsTranslationsRobot.() -> Unit): Transition {
            Log.i(TAG, "goBackToTranslationSettingsSubMenu: Trying to click the navigate up button")
            itemWithDescription(getStringResource(R.string.action_bar_up_description)).click()
            Log.i(TAG, "goBackToTranslationSettingsSubMenu: Clicked the navigate up button")

            SettingsTranslationsRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun goBackToTranslationOptionSheet(interact: TranslationsRobot.() -> Unit): TranslationsRobot.Transition {
            Log.i(TAG, "goBackToTranslationOptionSheet: Trying to click the navigate up button")
            itemWithDescription(getStringResource(R.string.action_bar_up_description)).click()
            Log.i(TAG, "goBackToTranslationOptionSheet: Clicked the navigate up button")

            TranslationsRobot(composeTestRule).interact()
            return TranslationsRobot.Transition(composeTestRule)
        }
    }
}
