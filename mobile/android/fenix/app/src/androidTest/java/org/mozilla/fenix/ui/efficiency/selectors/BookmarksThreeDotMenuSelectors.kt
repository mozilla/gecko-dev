package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object BookmarksThreeDotMenuSelectors {

    val OPEN_IN_NEW_TAB_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = getStringResource(R.string.bookmark_menu_open_in_new_tab_button),
        description = "Open in new tab bookmarks three dot menu button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        OPEN_IN_NEW_TAB_BUTTON,
    )
}
