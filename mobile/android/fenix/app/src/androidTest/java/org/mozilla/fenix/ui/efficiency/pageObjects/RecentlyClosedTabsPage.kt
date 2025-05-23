package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep
import org.mozilla.fenix.ui.efficiency.selectors.HistorySelectors
import org.mozilla.fenix.ui.efficiency.selectors.HomeSelectors
import org.mozilla.fenix.ui.efficiency.selectors.MainMenuSelectors
import org.mozilla.fenix.ui.efficiency.selectors.RecentlyClosedTabsSelectors

class RecentlyClosedTabsPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "RecentlyClosedTabsPage"

    init {
        NavigationRegistry.register(
            from = "HomePage",
            to = pageName,
            steps = listOf(
                NavigationStep.Click(HomeSelectors.MAIN_MENU_BUTTON),
                NavigationStep.Click(MainMenuSelectors.HISTORY_BUTTON),
                NavigationStep.Click(HistorySelectors.RECENTLY_CLOSED_TABS_BUTTON),
            ),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return RecentlyClosedTabsSelectors.all.filter { it.groups.contains(group) }
    }
}
