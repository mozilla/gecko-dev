package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsAboutTest : BaseTest() {

    @Test
    fun verifyAboutSettingsSectionTest() {
        on.settingsAbout.navigateToPage()
    }
}
