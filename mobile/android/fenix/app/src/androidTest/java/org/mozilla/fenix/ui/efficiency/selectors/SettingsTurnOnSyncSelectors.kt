package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsTurnOnSyncSelectors {

    val USE_EMAIL_INSTEAD_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Use email instead",
        description = "Use email instead button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        USE_EMAIL_INSTEAD_BUTTON,
    )
}
