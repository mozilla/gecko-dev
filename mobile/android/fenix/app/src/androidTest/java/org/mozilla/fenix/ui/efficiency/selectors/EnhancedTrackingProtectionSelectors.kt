package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object EnhancedTrackingProtectionSelectors {

    val ETP_QUICK_SETTINGS_SHEET = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "trackingProtectionLayout",
        description = "Enhanced tracking protection section from quick settings sheet",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        ETP_QUICK_SETTINGS_SHEET,
    )
}
