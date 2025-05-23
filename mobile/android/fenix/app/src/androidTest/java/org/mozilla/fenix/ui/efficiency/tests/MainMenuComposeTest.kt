package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class MainMenuComposeTest : BaseTest(
    isMenuRedesignEnabled = true,
) {

    @Test
    fun verifyMainMenuItemsTest() {
        on.mainMenuCompose.navigateToPage()
    }
}
