package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsEnhancedTrackingProtectionSelectors {

    val ENHANCED_TRACKING_PROTECTION_SUMMARY = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "$appName protects you from many of the most common trackers that follow what you do online.",
        description = "Enhanced tracking protection section summary",
        groups = listOf("requiredForPage"),
    )

    val ENHANCED_TRACKING_PROTECTION_EXCEPTIONS_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Exceptions",
        description = "Enhanced tracking protection Exceptions button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        ENHANCED_TRACKING_PROTECTION_SUMMARY,
        ENHANCED_TRACKING_PROTECTION_EXCEPTIONS_BUTTON,
    )
}
