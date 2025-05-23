package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object BookmarksSelectors {
    val TOOLBAR_TITLE = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = "Bookmarks",
        description = "Bookmarks Toolbar Title",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        TOOLBAR_TITLE,
    )
}
