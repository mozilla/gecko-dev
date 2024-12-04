/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.concept.AutocompletePrompt

/**
 * Delegate to display the login select prompt and related callbacks
 */
interface LoginDelegate {
    /**
     * The [AutocompletePrompt] used for [LoginPicker] to display a list of logins available to autocomplete.
     */
    val loginPickerView: AutocompletePrompt<Login>?
        get() = null

    /**
     * Callback invoked when a user selects "Manage logins"
     * from the select login prompt.
     */
    val onManageLogins: () -> Unit
        get() = {}
}
