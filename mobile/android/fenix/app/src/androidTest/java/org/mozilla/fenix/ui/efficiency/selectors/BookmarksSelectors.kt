package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object BookmarksSelectors {
    val TOOLBAR_TITLE = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = "Bookmarks",
        description = "Bookmarks Toolbar Title",
        groups = listOf("requiredForPage"),
    )

    val OPEN_IN_NEW_TAB_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = getStringResource(R.string.bookmark_menu_open_in_new_tab_button),
        description = "Open in new tab bookmarks three dot menu button",
        groups = listOf("bookmarksThreeDotMenu"),
    )

    val all = listOf(
        TOOLBAR_TITLE,
        OPEN_IN_NEW_TAB_BUTTON,
    )
}
