package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SystemSettingsSelectors {

    // Will need support for querying multiple values eg: res id and description (itemWithResIdAndDescription)
    val PRIVATE_BROWSING_SYSTEM_SETTINGS_NOTIFICATIONS_TOGGLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_DESCRIPTION_CONTAINS,
        value = "Private browsing session",
        description = "Private browsing system settings notifications toggle",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        PRIVATE_BROWSING_SYSTEM_SETTINGS_NOTIFICATIONS_TOGGLE,
    )
}
