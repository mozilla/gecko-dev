package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSiteSettingsSelectors {

    val TOOLBAR_TITLE = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Site settings",
        description = "Site settings toolbar title",
        groups = listOf("requiredForPage"),
    )

    val EXCEPTIONS_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Exceptions",
        description = "Site settings Exceptions button",
        groups = listOf("exceptions"),
    )

    val all = listOf(
        TOOLBAR_TITLE,
        EXCEPTIONS_BUTTON,
    )
}
