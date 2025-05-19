package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep
import org.mozilla.fenix.ui.efficiency.selectors.HomeSelectors

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
            to = "ThreeDotMenu",
            steps = listOf(NavigationStep.Click(HomeSelectors.THREE_DOT_MENU)),
        )

        NavigationRegistry.register(
            from = "ThreeDotMenu",
            to = "BookmarksPage",
            steps = listOf(NavigationStep.Click(HomeSelectors.TDM_BOOKMARKS_BUTTON)),
        )

        NavigationRegistry.register(
            from = "ThreeDotMenu",
            to = "SettingsPage",
            steps = listOf(
                NavigationStep.Swipe(HomeSelectors.TDM_SETTINGS_BUTTON_COMPOSE),
                NavigationStep.Click(HomeSelectors.TDM_SETTINGS_BUTTON_COMPOSE),
            ),
        )

        NavigationRegistry.register(
            from = "ThreeDotMenu",
            to = "HistoryPage",
            steps = listOf(NavigationStep.Click(HomeSelectors.TDM_HISTORY_BUTTON)),
        )

        NavigationRegistry.register(
            from = "ThreeDotMenu",
            to = "DownloadsPage",
            steps = listOf(NavigationStep.Click(HomeSelectors.TDM_DOWNLOADS_BUTTON)),
        )

        NavigationRegistry.register(
            from = "ThreeDotMenu",
            to = "PasswordsPage",
            steps = listOf(NavigationStep.Click(HomeSelectors.TDM_PASSWORDS_BUTTON)),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return HomeSelectors.all.filter { it.groups.contains(group) }
    }
}
