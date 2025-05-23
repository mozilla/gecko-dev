package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsDeleteBrowsingDataOnQuitSelectors {

    val DELETE_BROWSING_DATA_ON_QUIT_OPTION_SUMMARY = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Automatically deletes browsing data when you select “Quit” from the main menu",
        description = "Delete browsing data on quit option summary",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        DELETE_BROWSING_DATA_ON_QUIT_OPTION_SUMMARY,
    )
}
