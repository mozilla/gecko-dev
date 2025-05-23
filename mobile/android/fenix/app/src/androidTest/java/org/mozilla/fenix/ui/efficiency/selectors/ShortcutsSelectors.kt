package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object ShortcutsSelectors {

    val SHORTCUTS_DIALOG_ADD_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "add_button",
        description = "Shortcuts dialog add button",
        groups = listOf("shortcutsDialog"),
    )

    val all = listOf(
        SHORTCUTS_DIALOG_ADD_BUTTON,
    )
}
