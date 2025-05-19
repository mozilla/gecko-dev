package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsPasswordsSelectors {
    val SETTINGS_PASSWORDS_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Passwords",
        description = "The Passwords Settings title",
        groups = listOf("requiredForPage"),
    )

    val SAVE_PASSWORDS_TOGGLE = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "save_passwords_toggle",
        description = "Save Passwords Toggle",
        groups = listOf("passwordSettings"),
    )

    val all = listOf(
        SETTINGS_PASSWORDS_TITLE,
        SAVE_PASSWORDS_TOGGLE,
    )
}
