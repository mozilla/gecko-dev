package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.tabstray.TabsTrayTestTag
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object TabDrawerSelectors {

    val NORMAL_BROWSING_OPEN_TABS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TAG,
        value = TabsTrayTestTag.NORMAL_TABS_PAGE_BUTTON,
        description = "Normal browsing tabs tray button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NORMAL_BROWSING_OPEN_TABS_BUTTON,
    )
}
