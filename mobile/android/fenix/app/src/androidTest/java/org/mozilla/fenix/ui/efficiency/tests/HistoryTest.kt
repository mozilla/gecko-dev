package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class HistoryTest : BaseTest() {

    @Test
    fun verifyHistorySectionTest() {
        on.history.navigateToPage()
    }
}
