package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep
import org.mozilla.fenix.ui.efficiency.selectors.HomeSelectors
import org.mozilla.fenix.ui.efficiency.selectors.MainMenuSelectors
import org.mozilla.fenix.ui.efficiency.selectors.SettingsSelectors
import org.mozilla.fenix.ui.efficiency.selectors.SettingsTranslationSelectors

class SettingsTranslationsPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "SettingsTranslationsPage"

    init {
        NavigationRegistry.register(
            from = "HomePage",
            to = pageName,
            steps = listOf(
                NavigationStep.Click(HomeSelectors.MAIN_MENU_BUTTON),
                NavigationStep.Click(MainMenuSelectors.SETTINGS_BUTTON),
                NavigationStep.Click(SettingsSelectors.TRANSLATIONS_BUTTON),
            ),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return SettingsTranslationSelectors.all.filter { it.groups.contains(group) }
    }
}
