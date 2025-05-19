package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class HomeTest : BaseTest() {
    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/235396
    @Test
    fun homeScreenItemsTest() {
        // Given: App is loaded with default settings
        // on = AndroidComposeTestRule<HomeActivityIntentTestRule, *> with app defaults

        // When: We navigate to the Settings page and back to the Home page
        on.settings.navigateToPage()
        on.home.navigateToPage()

        // Then: the browser chrome, page components, and elements should load
        on.home.mozVerifyElementsByGroup("topSitesCompose")
    }
}
