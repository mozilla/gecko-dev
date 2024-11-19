/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Context
import android.content.pm.ActivityInfo
import org.json.JSONObject
import org.junit.Assert.assertNotEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.experiments.nimbus.HardcodedNimbusFeatures
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.nimbus.HomeScreenSection
import org.mozilla.fenix.nimbus.Homescreen
import org.mozilla.fenix.ui.robots.browserScreen

/**
 *  Tests for verifying basic functionality of the Nimbus Survey surface message
 *
 *  Verifies a message can be displayed with all of the correct components
**/

class NimbusMessagingSurveyTest : TestSetup() {
    private lateinit var context: Context
    private lateinit var hardcodedNimbus: HardcodedNimbusFeatures

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(
        skipOnboarding = true,
    )

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(2)

    @Before
    override fun setUp() {
        super.setUp()
        context = TestHelper.appContext

        // Set up nimbus message
        hardcodedNimbus = HardcodedNimbusFeatures(
            context,
            "messaging" to JSONObject(
                """
                {
                  "message-under-experiment": "test-survey-messaging-surface",
                  "messages": {
                    "test-survey-messaging-surface": {
                      "title": "Survey Message Test",
                      "text": "Some Nimbus Messaging text",
                      "surface": "survey",
                      "style": "SURVEY",
                      "action": "OPEN_URL",
                      "action-params": {
                        "url": "https://www.example.com"
                      },
                      "trigger": [
                        "ALWAYS"
                      ]
                    }
                  }
                }
                """.trimIndent(),
            ),
        )

        // Remove some homescreen features not needed for testing
        FxNimbus.features.homescreen.withInitializer { _, _ ->
            // These are FML generated objects and enums
            Homescreen(
                sectionsEnabled = mapOf(
                    HomeScreenSection.JUMP_BACK_IN to false,
                    HomeScreenSection.POCKET to false,
                    HomeScreenSection.POCKET_SPONSORED_STORIES to false,
                    HomeScreenSection.RECENT_EXPLORATIONS to false,
                    HomeScreenSection.BOOKMARKS to false,
                    HomeScreenSection.TOP_SITES to false,
                ),
            )
        }
        activityTestRule.finishActivity()
        hardcodedNimbus.connectWith(FxNimbus)
        activityTestRule.launchActivity(null)
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2809390
    @Test
    fun checkSurveyNavigatesCorrectly() {
        browserScreen {
            verifySurveyButton()
        }.clickSurveyButton {
            assertNotEquals("", getCurrentUrl())
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2809389
    @Test
    fun checkSurveyNoThanksNavigatesCorrectly() {
        browserScreen {
            verifySurveyNoThanksButton()
        }.clickNoThanksSurveyButton {
            verifyTabCounter("0")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2809388
    @Test
    fun checkSurveyLandscapeLooksCorrect() {
        activityTestRule.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        browserScreen {
            verifySurveyNoThanksButton()
            verifySurveyButton()
        }.clickNoThanksSurveyButton {
            verifyTabCounter("0")
        }
    }
}
