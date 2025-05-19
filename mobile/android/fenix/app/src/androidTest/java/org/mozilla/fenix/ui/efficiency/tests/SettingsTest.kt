package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsTest : BaseTest() {

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2092697
    @Test
    fun verifyGeneralSettingsItemsTest() {
        // Given: App is loaded with default settings
        // on = AndroidComposeTestRule<HomeActivityIntentTestRule, *> with app defaults

        // When: We navigate to the Settings page
        on.settings.navigateToPage()

        // Then: all elements should load
        // by default navigateToPage() asserts all 'requiredForPage' elements are present
    }
}
