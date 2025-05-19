package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsCustomizeSelectors {
    val SETTINGS_CUSTOMIZE_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Customize",
        description = "The Customize Settings title",
        groups = listOf("requiredForPage"),
    )

    val SHOW_TOOLBAR_TOGGLE = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "show_toolbar_toggle",
        description = "Show Toolbar Toggle",
        groups = listOf("customizeSettings"),
    )

    val all = listOf(
        SETTINGS_CUSTOMIZE_TITLE,
        SHOW_TOOLBAR_TOGGLE,
    )
}
