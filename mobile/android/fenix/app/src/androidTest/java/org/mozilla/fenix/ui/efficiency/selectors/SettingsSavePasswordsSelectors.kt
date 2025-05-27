package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSavePasswordsSelectors {

    val ASK_TO_SAVE_OPTION = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Ask to save",
        description = "Ask to save option",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        ASK_TO_SAVE_OPTION,
    )
}
