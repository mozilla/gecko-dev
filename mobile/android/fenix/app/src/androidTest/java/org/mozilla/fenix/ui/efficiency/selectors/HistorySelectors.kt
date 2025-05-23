package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object HistorySelectors {
    val TOOLBAR_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "History",
        description = "History Toolbar Title",
        groups = listOf("requiredForPage"),
    )

    val RECENTLY_CLOSED_TABS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "recently_closed_tabs_header",
        description = "Recently closed tabs button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        TOOLBAR_TITLE,
        RECENTLY_CLOSED_TABS_BUTTON,
    )
}
