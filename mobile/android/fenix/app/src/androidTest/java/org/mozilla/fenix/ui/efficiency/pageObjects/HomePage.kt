package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep
import org.mozilla.fenix.ui.efficiency.selectors.HomeSelectors
import org.mozilla.fenix.ui.efficiency.selectors.MainMenuSelectors

class HomePage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {

    override val pageName = "HomePage"

    init {
        NavigationRegistry.register(
            from = "AppEntry",
            to = pageName,
            steps = listOf(),
        )

        NavigationRegistry.register(
            from = pageName,
            to = "MainMenuPage",
            steps = listOf(NavigationStep.Click(HomeSelectors.MAIN_MENU_BUTTON)),
        )

        NavigationRegistry.register(
            from = "MainMenuPage",
            to = "BookmarksPage",
            steps = listOf(NavigationStep.Click(MainMenuSelectors.BOOKMARKS_BUTTON)),
        )

        NavigationRegistry.register(
            from = "MainMenuPage",
            to = "SettingsPage",
            steps = listOf(
                NavigationStep.Swipe(MainMenuSelectors.SETTINGS_BUTTON),
                NavigationStep.Click(MainMenuSelectors.SETTINGS_BUTTON),
            ),
        )

        NavigationRegistry.register(
            from = "MainMenuPage",
            to = "HistoryPage",
            steps = listOf(NavigationStep.Click(MainMenuSelectors.HISTORY_BUTTON)),
        )

        NavigationRegistry.register(
            from = "MainMenuPage",
            to = "DownloadsPage",
            steps = listOf(NavigationStep.Click(MainMenuSelectors.DOWNLOADS_BUTTON)),
        )

        NavigationRegistry.register(
            from = "MainMenuPage",
            to = "PasswordsPage",
            steps = listOf(NavigationStep.Click(MainMenuSelectors.PASSWORDS_BUTTON)),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return HomeSelectors.all.filter { it.groups.contains(group) }
    }
}
