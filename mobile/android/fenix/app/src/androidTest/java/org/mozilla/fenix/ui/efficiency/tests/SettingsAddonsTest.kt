package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsAddonsTest : BaseTest() {

    @Test
    fun verifyTheAddonsSectionTest() {
        on.settingsAddonsManager.navigateToPage()
    }
}
