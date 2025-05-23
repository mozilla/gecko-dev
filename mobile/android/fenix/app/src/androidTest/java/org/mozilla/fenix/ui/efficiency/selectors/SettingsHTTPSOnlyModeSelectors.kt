package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsHTTPSOnlyModeSelectors {

    val HTTPS_MODE_OPTION_SUMMARY = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Automatically attempts to connect to sites using HTTPS encryption protocol for increased security. Learn more",
        description = "HTTPS only mode option summary",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        HTTPS_MODE_OPTION_SUMMARY,
    )
}
