package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsSiteSettingsTest : BaseTest() {

    @Test
    fun verifySiteSettingsSectionTest() {
        on.settingsSiteSettings.navigateToPage()
    }

    @Test
    fun verifySiteSettingsExceptionsSectionTest() {
        on.settingsSiteSettingsExceptions.navigateToPage()
    }
}
