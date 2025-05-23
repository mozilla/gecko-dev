package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep
import org.mozilla.fenix.ui.efficiency.selectors.MainMenuSelectors
import org.mozilla.fenix.ui.efficiency.selectors.SettingsSelectors

class SettingsPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "SettingsPage"

    init {
        NavigationRegistry.register(
            from = "MainMenuPage",
            to = pageName,
            steps = listOf(NavigationStep.Click(MainMenuSelectors.SETTINGS_BUTTON)),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "HomePage",
            steps = listOf(NavigationStep.Click(SettingsSelectors.GO_BACK_BUTTON)),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "SettingsAccessibilityPage",
            steps = listOf(
                NavigationStep.Swipe(SettingsSelectors.ACCESSIBILITY_BUTTON),
                NavigationStep.Click(SettingsSelectors.ACCESSIBILITY_BUTTON),
            ),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "SettingsAutofillPage",
            steps = listOf(NavigationStep.Click(SettingsSelectors.AUTOFILL_BUTTON)),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "SettingsCustomizePage",
            steps = listOf(NavigationStep.Click(SettingsSelectors.CUSTOMIZE_BUTTON)),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "SettingsHomepagePage",
            steps = listOf(NavigationStep.Click(SettingsSelectors.HOMEPAGE_BUTTON)),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "SettingsPasswordsPage",
            steps = listOf(NavigationStep.Click(SettingsSelectors.PASSWORDS_BUTTON)),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "SettingsSearchPage",
            steps = listOf(NavigationStep.Click(SettingsSelectors.SEARCH_BUTTON)),
        )
        NavigationRegistry.register(
            from = pageName,
            to = "SettingsTabsPage",
            steps = listOf(NavigationStep.Click(SettingsSelectors.TABS_BUTTON)),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return SettingsSelectors.all.filter { it.groups.contains(group) }
    }
}
