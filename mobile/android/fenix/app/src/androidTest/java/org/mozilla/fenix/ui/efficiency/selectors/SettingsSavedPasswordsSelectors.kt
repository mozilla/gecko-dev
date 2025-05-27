package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSavedPasswordsSelectors {

    val LATER_DIALOG_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Later",
        description = "\"Secure your passwords\" Later dialog button",
        groups = listOf("securePasswordsDialog"),
    )

    val EMPTY_SAVED_PASSWORDS_LIST_DESCRIPTION = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = getStringResource(R.string.preferences_passwords_saved_logins_description_empty_text_2),
        description = "Save Passwords Toggle",
        groups = listOf("emptySavedPasswordsList"),
    )

    val EMPTY_SAVED_PASSWORDS_LIST_LEARN_MORE_ABOUT_SYNC = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Learn more about sync",
        description = "Save Passwords Toggle",
        groups = listOf("emptySavedPasswordsList"),
    )

    val EMPTY_SAVED_PASSWORDS_LIST_ADD_PASSWORD_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Add password",
        description = "Save Passwords Toggle",
        groups = listOf("emptySavedPasswordsList"),
    )

    val all = listOf(
        LATER_DIALOG_BUTTON,
        EMPTY_SAVED_PASSWORDS_LIST_DESCRIPTION,
        EMPTY_SAVED_PASSWORDS_LIST_LEARN_MORE_ABOUT_SYNC,
        EMPTY_SAVED_PASSWORDS_LIST_ADD_PASSWORD_BUTTON,
    )
}
