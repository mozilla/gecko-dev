/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.navigationToolbar

class AboutURITest : TestSetup() {
    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2944327
    @Test
    fun verifyWebCompatPageIsLoadingTest() {
        val webCompatPage = "about:compat"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webCompatPage.toUri()) {
            verifyUrl(webCompatPage)
            verifyPageContent("Interventions")
            verifyPageContent("More Information: Bug")
            scrollIntoViewTheSmartBlockFixesSection()
            verifyPageContent("SmartBlock Fixes")
            // Scroll down to be able to properly verify the bugs listed in the "SmartBlock Fixes section"
            scrollToTheEndOfTheAboutCompatPage()
            verifyPageContent("More Information: Bug")
        }
    }
}
