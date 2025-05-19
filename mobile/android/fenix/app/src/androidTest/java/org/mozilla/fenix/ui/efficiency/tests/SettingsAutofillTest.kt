package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsAutofillTest : BaseTest() {

    @Test
    fun verifySettingsAutofillLoadsTest() {
        on.settingsAutofill.navigateToPage()
    }
}
