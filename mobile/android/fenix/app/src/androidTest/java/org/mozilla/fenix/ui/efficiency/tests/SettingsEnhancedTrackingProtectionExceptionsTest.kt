package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsEnhancedTrackingProtectionExceptionsTest : BaseTest() {

    @Test
    fun verifyTheEnhancedTrackingProtectionExceptionsSectionTest() {
        on.settingsEnhancedTrackingProtectionExceptions.navigateToPage()
    }
}
