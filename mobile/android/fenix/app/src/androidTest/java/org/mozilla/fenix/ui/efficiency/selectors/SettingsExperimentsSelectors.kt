package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsExperimentsSelectors {

    val NAVIGATE_BACK_TOOLBAR_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_CONTENT_DESC,
        value = "Navigate up",
        description = "Navigate back toolbar button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NAVIGATE_BACK_TOOLBAR_BUTTON,
    )
}
