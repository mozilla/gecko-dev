package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsHTTPSOnlyModeTest : BaseTest() {

    @Test
    fun verifyTheHTTPSOnlyModeSectionTest() {
        on.settingsHTTPSOnlyMode.navigateToPage()
    }
}
