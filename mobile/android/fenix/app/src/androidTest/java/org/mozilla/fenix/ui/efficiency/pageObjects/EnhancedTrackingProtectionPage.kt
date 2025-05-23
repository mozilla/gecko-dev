package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.selectors.EnhancedTrackingProtectionSelectors

class EnhancedTrackingProtectionPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "EnhancedTrackingProtectionPage"

    init {
        NavigationRegistry.register(
            from = "BrowserPage",
            to = pageName,
            steps = listOf(
                // Will need to create selectors for different pages to have a nav path
            ),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return EnhancedTrackingProtectionSelectors.all.filter { it.groups.contains(group) }
    }
}
