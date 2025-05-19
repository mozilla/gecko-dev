package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsHomepageSelectors {
    val SETTINGS_HOMEPAGE_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Homepage",
        description = "The Homepage Settings menu item",
        groups = listOf("requiredForPage"),
    )

    val SHOW_TOP_SITES_TOGGLE = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "show_top_sites_toggle",
        description = "Show Top Sites Toggle",
        groups = listOf("homepageSettings"),
    )

    val all = listOf(
        SETTINGS_HOMEPAGE_TITLE,
        SHOW_TOP_SITES_TOGGLE,
    )
}
