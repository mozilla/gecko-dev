package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object FindInPageSelectors {

    val FIND_IN_PAGE_CLOSE_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "find_in_page_close_btn",
        description = "Find in page close button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        FIND_IN_PAGE_CLOSE_BUTTON,
    )
}
