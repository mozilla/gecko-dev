/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.assertIsOff
import androidx.compose.ui.test.assertIsOn
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withId
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemIsChecked
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Data Collection sub menu.
 */
class SettingsSubMenuDataCollectionRobot {

    fun verifyDataCollectionView(
        composeTestRule: ComposeTestRule,
        isSendTechnicalDataEnabled: Boolean,
        isDailyUsagePingEnabled: Boolean,
        studiesSummary: String,
        isAskBeforeSendingCrashReportsEnabled: Boolean = true,
        isAutomaticallySendCrashReportsEnabled: Boolean = false,
        isNeverSendCrashReportsEnabled: Boolean = false,
    ) {
        Log.i(TAG, "verifyDataCollectionView: Waiting for compose test rule to be idle")
        composeTestRule.waitForIdle()
        Log.i(TAG, "verifyDataCollectionView: Waited for compose test rule to be idle")
        assertUIObjectExists(
            // Toolbar items
            goBackButton(),
            itemContainingText(getStringResource(R.string.preferences_data_collection)),
            // Technical Data section
            itemContainingText(getStringResource(R.string.technical_data_category)),
            itemContainingText(getStringResource(R.string.preference_usage_data_2)),
            itemContainingText(getStringResource(R.string.preferences_usage_data_description_1)),
            itemWithDescription("Learn more about technical data Links available"),
            // Studies section
            itemContainingText(getStringResource(R.string.studies_data_category)),
            itemContainingText(getStringResource(R.string.studies_title)),
            itemContainingText(studiesSummary),
            // Usage data section
            itemContainingText(getStringResource(R.string.usage_data_category)),
            itemContainingText(getStringResource(R.string.preferences_daily_usage_ping_title)),
            itemContainingText(getStringResource(R.string.preferences_daily_usage_ping_description)),
            itemWithDescription("Learn more about daily usage ping Links available"),
            // Crash reports section
            itemContainingText(getStringResource(R.string.crash_reporting_description)),
            itemContainingText(getStringResource(R.string.crash_reporting_ask)),
            itemContainingText(getStringResource(R.string.crash_reporting_auto)),
            itemContainingText(getStringResource(R.string.crash_reporting_never)),
        )

        // Technical Data toggle
        verifyUsageAndTechnicalDataToggle(composeTestRule, isSendTechnicalDataEnabled)

        // Daily ping toggle
        verifyDailyUsagePingToggle(composeTestRule, isDailyUsagePingEnabled)

        // Crash reports radio buttons
        assertItemIsChecked(itemWithResId("data.collection.Ask.radio.button"), isChecked = isAskBeforeSendingCrashReportsEnabled)
        assertItemIsChecked(itemWithResId("data.collection.Auto.radio.button"), isChecked = isAutomaticallySendCrashReportsEnabled)
        assertItemIsChecked(itemWithResId("data.collection.Never.radio.button"), isChecked = isNeverSendCrashReportsEnabled)
    }

    fun verifyUsageAndTechnicalDataToggle(composeTestRule: ComposeTestRule, isChecked: Boolean) {
        Log.i(TAG, "verifyUsageAndTechnicalDataToggle: Trying to verify that the \"Technical and interaction data\" toggle is checked: $isChecked")
        if (isChecked) {
            composeTestRule.onNodeWithTag("data.collection.Send technical and interaction data.toggle", useUnmergedTree = true)
                .assertIsOn()
            Log.i(TAG, "verifyUsageAndTechnicalDataToggle: Verified that the \"Usage and technical data\" toggle is checked: $isChecked")
        } else {
            composeTestRule.onNodeWithTag("data.collection.Send technical and interaction data.toggle", useUnmergedTree = true)
                .assertIsOff()
            Log.i(TAG, "verifyUsageAndTechnicalDataToggle: Verified that the \"Usage and technical data\" toggle is checked: $isChecked")
        }
    }

    fun verifyDailyUsagePingToggle(composeTestRule: ComposeTestRule, isChecked: Boolean) {
        Log.i(TAG, "verifyDailyUsagePingToggle: Trying to verify that the \"Daily usage ping\" toggle is checked: $isChecked")
        if (isChecked) {
            composeTestRule.onNodeWithTag("data.collection.Daily usage ping.toggle", useUnmergedTree = true)
                .assertIsOn()
            Log.i(TAG, "verifyDailyUsagePingToggle: Verified that the \"Daily usage ping\" toggle is checked: $isChecked")
        } else {
            composeTestRule.onNodeWithTag("data.collection.Daily usage ping.toggle", useUnmergedTree = true)
                .assertIsOff()
            Log.i(TAG, "verifyDailyUsagePingToggle: Verified that the \"Daily usage ping\" toggle is checked: $isChecked")
        }
    }

    fun verifyStudiesToggle(enabled: Boolean) {
        Log.i(TAG, "verifyStudiesToggle: Trying to verify that the \"Studies\" toggle is checked: $enabled")
        onView(withId(R.id.studies_switch))
            .check(
                matches(
                    if (enabled) {
                        isChecked()
                    } else {
                        isNotChecked()
                    },
                ),
            )
        Log.i(TAG, "verifyStudiesToggle: Verified that the \"Studies\" toggle is checked: $enabled")
    }

    fun clickUsageAndTechnicalDataToggle() {
        Log.i(TAG, "clickUsageAndTechnicalDataToggle: Trying to click the \"Technical and interaction data\" toggle")
        itemContainingText(getStringResource(R.string.preference_usage_data_2)).click()
        Log.i(TAG, "clickUsageAndTechnicalDataToggle: Clicked the \"Technical and interaction data\" toggle")
    }

    fun clickDailyUsagePingToggle() {
        Log.i(TAG, "clickDailyUsagePingToggle: Trying to click the \"Daily usage ping\" toggle")
        itemContainingText(getStringResource(R.string.preferences_daily_usage_ping_title)).click()
        Log.i(TAG, "clickDailyUsagePingToggle: Clicked the \"Daily usage ping\" toggle")
    }

    fun clickStudiesOption() {
        Log.i(TAG, "clickStudiesOption: Trying to click the \"Studies\" option")
        itemContainingText(getStringResource(R.string.studies_title)).click()
        Log.i(TAG, "clickStudiesOption: Clicked the \"Studies\" option")
    }

    fun clickStudiesToggle() {
        Log.i(TAG, "clickStudiesToggle: Trying to click the \"Studies\" toggle")
        itemWithResId("$packageName:id/studies_switch").click()
        Log.i(TAG, "clickStudiesToggle: Clicked the \"Studies\" toggle")
    }

    fun clickStudiesDialogOkButton() {
        Log.i(TAG, "clickStudiesDialogOkButton: Trying to click the \"Studies\" dialog \"Ok\" button")
        studiesDialogOkButton().click()
        Log.i(TAG, "clickStudiesDialogOkButton: Clicked the \"Studies\" dialog \"Ok\" button")
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up toolbar button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun goBackButton() = itemWithDescription("Navigate up")
private fun studiesDialogOkButton() = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())
private fun studiesDialogCancelButton() = onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog())
