/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experimentintegration

import android.content.pm.ActivityInfo
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.surveyScreen

/**
 *  Tests for verifying functionality of the message survey surface
 */
class SurveyExperimentIntegrationTest {
    private val surveyURL = "qsurvey.mozilla.com"
    private val experimentName = "Viewpoint"

    @get:Rule
    val activityTestRule = HomeActivityTestRule(
        isPWAsPromptEnabled = false,
        isDeleteSitePermissionsEnabled = true,
    )

    @Before
    fun setUp() {
        TestHelper.appContext.settings().showSecretDebugMenuThisSession = true
    }

    @After
    fun tearDown() {
        TestHelper.appContext.settings().showSecretDebugMenuThisSession = false
    }

    fun checkExperimentExists() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openExperimentsMenu {
            verifyExperimentExists(experimentName)
        }
    }

    @Test
    fun checkSurveyNavigatesCorrectly() {
        surveyScreen {
            verifySurveyButton()
        }.clickSurveyButton {
            verifyUrl(surveyURL)
        }

        checkExperimentExists()
    }

    @Test
    fun checkSurveyNoThanksNavigatesCorrectly() {
        surveyScreen {
            verifySurveyNoThanksButton()
        }.clickNoThanksSurveyButton {
            verifyTabCounter("0")
        }

        checkExperimentExists()
    }

    @Test
    fun checkHomescreenSurveyDismissesCorrectly() {
        surveyScreen {
            verifyHomeScreenSurveyCloseButton()
        }.clickHomeScreenSurveyCloseButton {
            verifyTabCounter("0")
            verifySurveyButtonDoesNotExist()
        }

        checkExperimentExists()
    }

    @Test
    fun checkSurveyLandscapeLooksCorrect() {
        activityTestRule.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        surveyScreen {
            verifySurveyNoThanksButton()
            verifySurveyButton()
        }.clickNoThanksSurveyButton {
            verifyTabCounter("0")
        }
    }
}
