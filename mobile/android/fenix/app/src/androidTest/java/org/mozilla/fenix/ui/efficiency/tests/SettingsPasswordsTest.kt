package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsPasswordsTest : BaseTest() {

    @Test
    fun verifySettingsPasswordsLoadsTest() {
        on.settingsPasswords.navigateToPage()
    }

    @Test
    fun verifySettingsSavePasswordsSectionTest() {
        on.settingsSavePasswords.navigateToPage()
    }
}
