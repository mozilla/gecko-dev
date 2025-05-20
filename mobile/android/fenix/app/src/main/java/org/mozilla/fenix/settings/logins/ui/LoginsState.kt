/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import mozilla.components.lib.state.State

/**
 * Represents the state of the Logins list screen and its various subscreens.
 *
 * @property loginItems Login items to be displayed in the current list screen.
 * @property searchText The text to filter login items.
 * @property sortOrder The order to display the login items.
 * @property biometricAuthenticationDialogState State representing the biometric authentication state.
 * @property loginsListState State representing the list login subscreen, if visible.
 * @property loginsAddLoginState State representing the add login subscreen, if visible.
 * @property loginsEditLoginState State representing the edit login subscreen, if visible.
 * @property loginsLoginDetailState State representing the login detail subscreen, if visible.
 * @property loginsDeletionDialogState State representing the deletion dialog state.
 */
internal data class LoginsState(
    val loginItems: List<LoginItem> = listOf(),
    val searchText: String? = null,
    val sortOrder: LoginsSortOrder = LoginsSortOrder.default,
    val biometricAuthenticationDialogState: BiometricAuthenticationDialogState? =
        BiometricAuthenticationDialogState.None,
    val loginsListState: LoginsListState? = null,
    val loginsAddLoginState: LoginsAddLoginState? = null,
    val loginsEditLoginState: LoginsEditLoginState? = null,
    val loginsLoginDetailState: LoginsLoginDetailState? = null,
    val loginsDeletionDialogState: DeletionDialogState? = null,
) : State

internal sealed class BiometricAuthenticationDialogState {
    data object None : BiometricAuthenticationDialogState()
    data object Authorized : BiometricAuthenticationDialogState()
    data object NonAuthorized : BiometricAuthenticationDialogState()
}

internal sealed class DeletionDialogState {
    data object None : DeletionDialogState()
    data class Presenting(
        val guidToDelete: String,
    ) : DeletionDialogState()
}

internal data class LoginsListState(
    val logins: List<LoginItem>,
)

internal data class LoginsEditLoginState(
    val login: LoginItem,
)

internal sealed class LoginsAddLoginState {
    data object None : LoginsAddLoginState()
    data class Presenting(val login: LoginItem) : LoginsAddLoginState()
}

internal data class LoginsLoginDetailState(
    val login: LoginItem,
)

/**
 * Represents the order of the Logins list items.
 */
sealed class LoginsSortOrder {
    abstract val asString: String
    abstract val comparator: Comparator<LoginItem>

    /**
     *  Represents the ordering of the logins list when sorted alphabetically.
     */
    data object Alphabetical : LoginsSortOrder() {
        override val asString: String
            get() = "alphabetical"

        override val comparator: Comparator<LoginItem>
            get() = compareBy { it.url }
    }

    /**
     *  Represents the ordering of the logins list when sorted by the last used date.
     */
    data object LastUsed : LoginsSortOrder() {
        override val asString: String
            get() = "last-used"

        override val comparator: Comparator<LoginItem>
            get() = compareByDescending { it.timeLastUsed }
    }

    /**
     *  Represents the [LoginsSortOrder] object.
     */
    companion object {
        val default: LoginsSortOrder
            get() = Alphabetical

        /**
         *  Converts a string into a [LoginsSortOrder] object.
         */
        fun fromString(value: String, default: LoginsSortOrder = Alphabetical): LoginsSortOrder {
            return when (value) {
                "alphabetical" -> Alphabetical
                "last-used" -> LastUsed
                else -> default
            }
        }
    }
}
