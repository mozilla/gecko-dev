package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsPrivateBrowsingTest : BaseTest() {

    @Test
    fun verifyTheSettingsPrivateBrowsingTest() {
        on.settingsPrivateBrowsing.navigateToPage()
    }
}
