/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsNotDisplayed
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Direction
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemIsChecked
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectIsGone
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName

class TranslationsRobot(private val composeTestRule: ComposeTestRule) {

    @OptIn(ExperimentalTestApi::class)
    fun verifyTranslationSheetIsDisplayed(isDisplayed: Boolean) {
        Log.i(TAG, "verifyTranslationSheetIsDisplayed: Trying to verify the Translations sheet is displayed $isDisplayed.")
        if (isDisplayed) {
            composeTestRule.waitUntilAtLeastOneExists(hasText("Translate to"), waitingTime)
            composeTestRule.onNodeWithText("Translate to").assertIsDisplayed()
        } else {
            composeTestRule.onNodeWithText("Translate to").assertIsNotDisplayed()
        }
        Log.i(TAG, "verifyTranslationSheetIsDisplayed: Verified the Translations sheet is displayed $isDisplayed.")
    }

    fun closeTranslationsSheet() {
        Log.i(TAG, "closeTranslationsSheet: Trying to close the Translations sheet.")
        itemWithResId("$packageName:id/touch_outside").click()
        Log.i(TAG, "closeTranslationsSheet: Closed the Translations sheet.")
    }

    fun clickTranslationsOptionsButton() {
        Log.i(TAG, "clickTranslationsOptionsButton: Trying to click the translations option button.")
        composeTestRule.onNodeWithContentDescription(getStringResource(R.string.translation_option_bottom_sheet_title_heading)).performClick()
        Log.i(TAG, "clickTranslationsOptionsButton: Clicked the translations options button.")
    }

    fun clickNeverTranslateLanguageOption(languageToTranslate: String) {
        Log.i(TAG, "clickNeverTranslateLanguageOption: Trying to click the \"Never translate $languageToTranslate\" option button.")
        composeTestRule.onNodeWithText("Never translate $languageToTranslate").performClick()
        Log.i(TAG, "clickNeverTranslateLanguageOption: Clicked the \"Never translate $languageToTranslate\" options button.")
    }

    fun verifyTheNeverTranslateLanguageDescription() {
        Log.i(TAG, "verifyTheNeverTranslateLanguageDescription: Trying to verify the \"Overrides offers to translate\" description is displayed.")
        composeTestRule.onNodeWithText(getStringResource(R.string.translation_option_bottom_sheet_switch_description)).assertIsDisplayed()
        Log.i(TAG, "verifyTheNeverTranslateLanguageDescription: Verified the \"Overrides offers to translate\" description is displayed.")
    }

    fun verifyTheNeverTranslateLanguageOptionState(isChecked: Boolean) {
        Log.i(TAG, "verifyTheNeverTranslateLanguageOptionState: Trying to verify the \"Overrides offers to translate\" description is displayed.")
        assertItemIsChecked(
            mDevice.findObject(
                UiSelector()
                    .index(5)
                    .className("android.view.View"),
            ),
            isChecked = isChecked,
        )
        Log.i(TAG, "verifyTheNeverTranslateLanguageOptionState: Verified the \"Overrides offers to translate\" description is displayed.")
    }

    class Transition(private val composeTestRule: ComposeTestRule) {
        fun clickTranslateButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickTranslateButton: Trying to click the Translate button from the Translations sheet.")
            composeTestRule.onNodeWithText("Translate").performClick()
            Log.i(TAG, "clickTranslateButton: Clicked the Translate button.")
            if (!itemWithDescription("Close Translations sheet").waitUntilGone(waitingTime)) {
                Log.i(TAG, "clickTranslateButton: Translate sheet is still displayed. Trying to close it.")
                TranslationsRobot(composeTestRule).closeTranslationsSheet()
                Log.i(TAG, "clickTranslateButton: Closed the Translations sheet.")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShowOriginalButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickShowOriginalButton: Trying to click on the Show Original button.")
            composeTestRule.onNodeWithText("Show original").performClick()
            Log.i(TAG, "clickShowOriginalButton: Clicked on the Show Original button.")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickNotNowButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickShowOriginalButton: Trying to click on the \"Not now\" button.")
            composeTestRule.onNodeWithText(getStringResource(R.string.translations_bottom_sheet_negative_button)).performClick()
            Log.i(TAG, "clickShowOriginalButton: Clicked on the \"Not now\" button.")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun swipeCloseTranslationsSheet(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            var retries = 0
            while (itemWithResId("$packageName:id/design_bottom_sheet").exists() && retries++ < 3) {
                Log.i(TAG, "swipeCloseTranslationsSheet: Started try #$retries")
                try {
                    Log.i(TAG, "swipeCloseTranslationsSheet: Trying to swipe down the Translations sheet.")
                    mDevice.findObject(By.res("$packageName:id/design_bottom_sheet")).swipe(Direction.DOWN, 1.0f)
                    Log.i(TAG, "swipeCloseTranslationsSheet: Swiped down the Translations sheet.")
                    assertUIObjectIsGone(itemWithResId("$packageName:id/design_bottom_sheet"))

                    break
                } catch (e: AssertionError) {
                    Log.i(TAG, "swipeCloseTranslationsSheet: AssertionError caught, executing fallback methods")
                    if (retries == 3) {
                        throw e
                    }
                }
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickTranslationSettingsButton(interact: SettingsTranslationsRobot.() -> Unit): SettingsTranslationsRobot.Transition {
            Log.i(TAG, "clickTranslationSettingsButton: Trying to click the \"Translation settings\" button.")
            composeTestRule.onNodeWithText(getStringResource(R.string.translation_option_bottom_sheet_translation_settings)).performClick()
            Log.i(TAG, "clickTranslationSettingsButton: Clicked the \"Translation settings\" button.")

            SettingsTranslationsRobot(composeTestRule).interact()
            return SettingsTranslationsRobot.Transition(composeTestRule)
        }
    }
}

fun translationsRobot(composeTestRule: ComposeTestRule, interact: TranslationsRobot.() -> Unit): TranslationsRobot.Transition {
    TranslationsRobot(composeTestRule).interact()
    return TranslationsRobot.Transition(composeTestRule)
}
