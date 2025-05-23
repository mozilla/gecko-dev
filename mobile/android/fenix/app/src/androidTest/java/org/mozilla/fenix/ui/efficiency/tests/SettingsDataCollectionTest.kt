package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsDataCollectionTest : BaseTest() {

    @Test
    fun verifyTheDataCollectionSettingsSectionTest() {
        on.settingsDataCollection.navigateToPage()
    }
}
