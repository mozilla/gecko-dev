package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class RecentlyClosedTabsTest : BaseTest() {

    @Test
    fun verifyEmptyRecentlyClosedTabsSectionTest() {
        on.recentlyClosedTabs.navigateToPage()
    }
}
