package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSearchSelectors {
    val SETTINGS_SEARCH_TITLE = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Search",
        description = "the Settings Search title",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(SETTINGS_SEARCH_TITLE)
}
