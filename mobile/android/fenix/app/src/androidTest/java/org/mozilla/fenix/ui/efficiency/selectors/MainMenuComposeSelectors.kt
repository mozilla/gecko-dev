package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object MainMenuComposeSelectors {

    val NEW_PRIVATE_TAB_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = getStringResource(R.string.browser_menu_new_private_tab),
        description = "Main menu New private tab button",
        // Removed in https://bugzilla.mozilla.org/show_bug.cgi?id=1966222 as part of the menu redesign effort
        groups = listOf("removedIn=141"),
    )

    val BOOKMARKS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = "Bookmarks",
        description = "Main menu Bookmarks button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NEW_PRIVATE_TAB_BUTTON,
        BOOKMARKS_BUTTON,
    )
}
