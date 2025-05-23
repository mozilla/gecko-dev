package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SettingsLanguageTest : BaseTest() {

    @Test
    fun verifyTheSettingsLanguageSectionTest() {
        on.settingsLanguage.navigateToPage()
    }
}
