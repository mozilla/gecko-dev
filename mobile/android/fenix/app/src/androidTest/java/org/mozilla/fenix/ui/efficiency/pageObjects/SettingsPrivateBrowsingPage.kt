package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep
import org.mozilla.fenix.ui.efficiency.selectors.HomeSelectors
import org.mozilla.fenix.ui.efficiency.selectors.MainMenuSelectors
import org.mozilla.fenix.ui.efficiency.selectors.SettingsPrivateBrowsingSelectors
import org.mozilla.fenix.ui.efficiency.selectors.SettingsSelectors

class SettingsPrivateBrowsingPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "SettingsPrivateBrowsingPage"

    init {
        NavigationRegistry.register(
            from = "HomePage",
            to = pageName,
            steps = listOf(
                NavigationStep.Click(HomeSelectors.MAIN_MENU_BUTTON),
                NavigationStep.Click(MainMenuSelectors.SETTINGS_BUTTON),
                NavigationStep.Swipe(SettingsSelectors.PRIVATE_BROWSING_BUTTON),
                NavigationStep.Click(SettingsSelectors.PRIVATE_BROWSING_BUTTON),
            ),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return SettingsPrivateBrowsingSelectors.all.filter { it.groups.contains(group) }
    }
}
