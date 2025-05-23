package org.mozilla.fenix.ui.efficiency.pageObjects

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.helpers.BasePage
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep
import org.mozilla.fenix.ui.efficiency.selectors.BookmarksThreeDotMenuSelectors
import org.mozilla.fenix.ui.efficiency.selectors.HomeSelectors
import org.mozilla.fenix.ui.efficiency.selectors.MainMenuSelectors

class BookmarksThreeDotMenuPage(composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) : BasePage(composeRule) {
    override val pageName = "BookmarksThreeDotMenuPage"

    init {
        NavigationRegistry.register(
            from = "HomePage",
            to = pageName,
            steps = listOf(
                NavigationStep.Click(HomeSelectors.MAIN_MENU_BUTTON),
                NavigationStep.Click(MainMenuSelectors.BOOKMARKS_BUTTON),
                // Will need to be able to click the bookmarked item's 3dot button
            ),
        )
    }

    override fun mozGetSelectorsByGroup(group: String): List<Selector> {
        return BookmarksThreeDotMenuSelectors.all.filter { it.groups.contains(group) }
    }
}
