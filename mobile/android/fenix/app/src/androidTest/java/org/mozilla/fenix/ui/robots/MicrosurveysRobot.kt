/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.ui.robots

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeUp
import androidx.test.espresso.Espresso.pressBack
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice

@OptIn(ExperimentalTestApi::class)
class MicrosurveysRobot {
    fun verifySurveyButton() = assertUIObjectExists(itemContainingText(getStringResource(R.string.preferences_take_survey)))

    fun verifySurveyNoThanksButton() =
        assertUIObjectExists(
            itemContainingText(getStringResource(R.string.preferences_not_take_survey)),
        )

    fun verifyHomeScreenSurveyCloseButton() =
        assertUIObjectExists(itemWithDescription("Close"))

    fun verifyContinueSurveyButton(composeTestRule: ComposeTestRule) {
        composeTestRule.waitUntilAtLeastOneExists(hasText(getStringResource(R.string.micro_survey_continue_button_label)), waitingTime)
        composeTestRule.continueSurveyButton().assertIsDisplayed()
    }

    fun clickContinueSurveyButton(composeTestRule: ComposeTestRule) {
        composeTestRule.waitUntilAtLeastOneExists(hasText(getStringResource(R.string.micro_survey_continue_button_label)), waitingTime)
        composeTestRule.continueSurveyButton().performClick()
    }

    fun verifyTheFirefoxLogo(composeTestRule: ComposeTestRule) {
        composeTestRule.waitUntilAtLeastOneExists(hasContentDescription("Firefox logo"), waitingTime)
        composeTestRule.onNodeWithContentDescription("Firefox logo").assertIsDisplayed()
    }

    fun verifyTheSurveyTitle(title: String, composeTestRule: ComposeTestRule) {
        composeTestRule.waitUntilAtLeastOneExists(hasText(title), waitingTime)
        composeTestRule.onNodeWithText(title).assertIsDisplayed()
    }

    fun verifyPleaseCompleteTheSurveyHeader(composeTestRule: ComposeTestRule) {
        composeTestRule.waitForIdle()
        composeTestRule.onNodeWithText(getStringResource(R.string.micro_survey_survey_header_2))
            .assertIsDisplayed()
    }

    fun expandSurveySheet(composeTestRule: ComposeTestRule) {
        composeTestRule
            .onNode(hasAnyChild(hasText(getStringResource(R.string.micro_survey_survey_header_2))))
            .performTouchInput { swipeUp() }
        composeTestRule.waitForIdle()
    }

    fun selectAnswer(answer: String, composeTestRule: ComposeTestRule) {
        composeTestRule.waitUntilAtLeastOneExists(hasText(answer), waitingTime)
        composeTestRule.onNodeWithText(answer).performClick()
    }

    fun clickSubmitButton(composeTestRule: ComposeTestRule) {
        composeTestRule.waitForIdle()
        composeTestRule.submitButton()
            .assertIsEnabled()
            .performClick()
    }

    fun verifySurveyCompletedScreen(composeTestRule: ComposeTestRule) {
        verifyPleaseCompleteTheSurveyHeader(composeTestRule)
        verifyTheFirefoxLogo(composeTestRule)
        assertUIObjectExists(homescreenSurveyCloseButton())
        composeTestRule.onNodeWithText(getStringResource(R.string.micro_survey_feedback_confirmation)).assertIsDisplayed()
        composeTestRule.privacyNoticeLink().assertIsDisplayed()
    }

    class Transition {
        fun clickSurveyButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            surveyButton().waitForExists(waitingTime)
            surveyButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickNoThanksSurveyButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            surveyNoThanksButton().waitForExists(waitingTime)
            surveyNoThanksButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickHomeScreenSurveyCloseButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            homescreenSurveyCloseButton().waitForExists(waitingTime)
            homescreenSurveyCloseButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun collapseSurveyByTappingBackButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            pressBack()
            mDevice.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun surveyScreen(interact: MicrosurveysRobot.() -> Unit): MicrosurveysRobot.Transition {
    MicrosurveysRobot().interact()
    return MicrosurveysRobot.Transition()
}

private fun surveyButton() =
    itemContainingText(getStringResource(R.string.preferences_take_survey))

private fun surveyNoThanksButton() =
    itemContainingText(getStringResource(R.string.preferences_not_take_survey))

private fun homescreenSurveyCloseButton() =
    itemWithDescription("Close")

private fun ComposeTestRule.continueSurveyButton() =
    onNodeWithText(getStringResource(R.string.micro_survey_continue_button_label))

private fun ComposeTestRule.submitButton() =
    onNodeWithText(getStringResource(R.string.micro_survey_submit_button_label))

private fun ComposeTestRule.privacyNoticeLink() =
    onNodeWithContentDescription("Privacy notice Links available")
