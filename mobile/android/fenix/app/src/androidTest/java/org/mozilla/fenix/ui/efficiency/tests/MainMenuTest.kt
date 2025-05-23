package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class MainMenuTest : BaseTest() {

    @Test
    fun verifyMainMenuItemsTest() {
        on.mainMenu.navigateToPage()
    }
}
