package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.selectors.ReaderViewSelectors

class ReaderViewPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "ReaderViewPage"

    init {
        NavigationRegistry.register(
            // Will need the BrowserPage
            from = "Browser",
            to = pageName,
            steps = listOf(),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return ReaderViewSelectors.all.filter { it.groups.contains(group) }
    }
}
