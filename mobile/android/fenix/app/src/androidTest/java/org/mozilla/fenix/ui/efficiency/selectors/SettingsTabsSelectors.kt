package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsTabsSelectors {
    val SETTINGS_TABS_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Tabs",
        description = "The Settings Tabs title",
        groups = listOf("requiredForPage"),
    )

    val NEW_TAB_PAGE_TOGGLE = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "new_tab_page_toggle",
        description = "New Tab Page Toggle Switch",
        groups = listOf("tabSettings"),
    )

    val all = listOf(
        SETTINGS_TABS_TITLE,
        NEW_TAB_PAGE_TOGGLE,
    )
}
