package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsLanguageSelectors {

    val SEARCH_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "search",
        description = "Language toolbar search button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        SEARCH_BUTTON,
    )
}
