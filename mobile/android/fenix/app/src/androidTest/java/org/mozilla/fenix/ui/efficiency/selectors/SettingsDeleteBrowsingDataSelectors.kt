package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsDeleteBrowsingDataSelectors {

    val DELETE_BROWSING_DATA_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "delete_data",
        description = "Delete browsing data button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        DELETE_BROWSING_DATA_BUTTON,
    )
}
