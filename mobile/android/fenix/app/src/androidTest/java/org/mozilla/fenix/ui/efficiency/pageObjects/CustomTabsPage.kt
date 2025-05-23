package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.selectors.CustomTabsSelectors

class CustomTabsPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "CustomTabsPage"

    init {
        NavigationRegistry.register(
            from = "HomePage",
            to = pageName,
            steps = listOf(
                // The custom tab is created and launched using the intentReceiverActivityTestRule which will create a createCustomTabIntent
            ),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return CustomTabsSelectors.all.filter { it.groups.contains(group) }
    }
}
