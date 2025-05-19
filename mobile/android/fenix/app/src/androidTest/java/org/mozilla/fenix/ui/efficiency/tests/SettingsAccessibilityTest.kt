package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsAccessibilityTest : BaseTest() {

    @Test
    fun verifySettingsAccessibilityPageLoadsTest() {
        on.settingsAccessibility.navigateToPage()
    }
}
