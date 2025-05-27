package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSiteSettingsExceptionsSelectors {

    val TOOLBAR_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Exceptions",
        description = "Site settings toolbar title",
        groups = listOf("requiredForPage"),
    )

    val EMPTY_EXCEPTIONS_LIST = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "No site exceptions",
        description = "Empty site settings exceptions list",
        groups = listOf("emptySiteSettingsExceptionsList"),
    )

    val all = listOf(
        TOOLBAR_TITLE,
        EMPTY_EXCEPTIONS_LIST,
    )
}
