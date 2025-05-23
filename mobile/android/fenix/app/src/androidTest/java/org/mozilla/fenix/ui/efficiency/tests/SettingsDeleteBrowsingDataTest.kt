package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsDeleteBrowsingDataTest : BaseTest() {

    @Test
    fun verifyTheDeleteBrowsingDataSectionTest() {
        on.settingsDeleteBrowsingData.navigateToPage()
    }
}
