package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object RecentlyClosedTabsSelectors {

    val EMPTY_RECENTLY_CLOSED_TABS_LIST = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "recently_closed_empty_view",
        description = "Recently closed tabs list",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        EMPTY_RECENTLY_CLOSED_TABS_LIST,
    )
}
