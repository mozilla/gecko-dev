/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.helpers

import android.util.Log
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Before
import org.junit.Rule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestSetup

abstract class BaseTest(
    private val skipOnboarding: Boolean = true,
    private val isMenuRedesignEnabled: Boolean = false,
    private val isMenuRedesignCFREnabled: Boolean = false,
    private val isPageLoadTranslationsPromptEnabled: Boolean = false,
) : TestSetup() {

    @get:Rule(order = 0)
    val composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *> =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                skipOnboarding = skipOnboarding,
                isMenuRedesignEnabled = isMenuRedesignEnabled,
                isMenuRedesignCFREnabled = isMenuRedesignCFREnabled,
                isPageLoadTranslationsPromptEnabled = isPageLoadTranslationsPromptEnabled,
            ),
        ) { it.activity }

    protected val on: PageContext = PageContext(composeRule)

    @get:Rule(order = 1)
    val retryTestRule = RetryTestRule(3)

    @Before
    override fun setUp() {
        super.setUp()
        PageStateTracker.currentPageName = "AppEntry"
        Log.i("BaseTest", "🚀 Starting test with page: AppEntry")
    }
}
