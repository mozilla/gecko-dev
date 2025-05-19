package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsCustomizeTest : BaseTest() {

    @Test
    fun verifySettingsCustomizeLoadsTest() {
        on.settingsCustomize.navigateToPage()
    }
}
