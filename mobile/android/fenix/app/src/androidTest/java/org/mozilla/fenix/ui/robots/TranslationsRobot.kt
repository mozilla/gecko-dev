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
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.packageName

class TranslationsRobot {

    @OptIn(ExperimentalTestApi::class)
    fun verifyTranslationSheetIsDisplayed(composeTestRule: ComposeTestRule, isDisplayed: Boolean) {
        Log.i(TAG, "verifyTranslationSheetIsDisplayed: Trying to verify the Translations sheet is displayed $isDisplayed.")
        composeTestRule.waitUntilAtLeastOneExists(hasText("Translate to"), waitingTime)
        composeTestRule.onNodeWithText("Translate to").apply {
            if (isDisplayed) assertIsDisplayed() else assertIsNotDisplayed()
        }
        Log.i(TAG, "verifyTranslationSheetIsDisplayed: Verified the Translations sheet is displayed $isDisplayed.")
    }

    fun closeTranslationsSheet() {
        Log.i(TAG, "closeTranslationsSheet: Trying to close the Translations sheet.")
//        composeTestRule
//            .onNodeWithContentDescription("Close Translations sheet")
//            .performClick()
        itemWithResId("$packageName:id/touch_outside").click()

        Log.i(TAG, "closeTranslationsSheet: Closed the Translations sheet.")
    }

    class Transition {
        fun clickTranslateButton(composeTestRule: ComposeTestRule, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickTranslateButton: Trying to click the Translate button from the Translations sheet.")
            composeTestRule.onNodeWithText("Translate").performClick()
            Log.i(TAG, "clickTranslateButton: Clicked the Translate button.")
            if (!itemWithDescription("Close Translations sheet").waitUntilGone(waitingTime)) {
                Log.i(TAG, "clickTranslateButton: Translate sheet is still displayed. Trying to close it.")
                TranslationsRobot().closeTranslationsSheet()
                Log.i(TAG, "clickTranslateButton: Closed the Translations sheet.")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShowOriginalButton(composeTestRule: ComposeTestRule, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickShowOriginalButton: Trying to click on the Show Original button.")
            composeTestRule.onNodeWithText("Show original").performClick()
            Log.i(TAG, "clickShowOriginalButton: Clicked on the Show Original button.")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}
