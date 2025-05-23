package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsEnhancedTrackingProtectionExceptionsSelectors {

    val LEARN_MORE_LINK = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Learn more",
        description = "Learn more link",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        LEARN_MORE_LINK,
    )
}
