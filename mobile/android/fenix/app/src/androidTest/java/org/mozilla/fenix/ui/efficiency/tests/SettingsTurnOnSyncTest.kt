package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsTurnOnSyncTest : BaseTest() {

    @Test
    fun verifyTurnOnSyncSectionTest() {
        on.settingsTurnOnSync.navigateToPage()
    }
}
