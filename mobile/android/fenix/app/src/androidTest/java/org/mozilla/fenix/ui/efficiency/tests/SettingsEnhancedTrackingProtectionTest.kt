package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsEnhancedTrackingProtectionTest : BaseTest() {

    @Test
    fun verifyTheEnhancedTrackingProtectionSectionTest() {
        on.settingsEnhancedTrackingProtection.navigateToPage()
    }
}
