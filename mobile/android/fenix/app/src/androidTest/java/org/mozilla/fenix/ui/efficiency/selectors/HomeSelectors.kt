package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object HomeSelectors {
    val TOP_SITES_LIST = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "top_sites_list",
        description = "Top Sites List",
        groups = listOf("topSites"),
    )

    val TOP_SITES_LIST_COMPOSE = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TAG,
        value = "top_sites_list",
        description = "Top Sites List",
        groups = listOf("topSitesCompose"),
    )

    val MAIN_MENU_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "menuButton",
        description = "Three Dot Menu",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        MAIN_MENU_BUTTON,
        TOP_SITES_LIST,
    )
}
