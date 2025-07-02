/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

/**
 * Function for reducing a new logins state based on the received action.
 */
internal fun loginsReducer(state: LoginsState, action: LoginsAction) = when (action) {
    is LoginsLoaded -> {
        state.handleLoginsLoadedAction(action)
    }
    is LoginsListSortMenuAction -> {
        state.handleSortMenuAction(action)
    }
    is SearchLogins -> {
        state.handleSearchLogins(action)
    }
    is LoginClicked -> state.copy(loginsLoginDetailState = LoginsLoginDetailState(action.item))
    is LoginsDetailBackClicked -> state.respondToLoginsDetailBackClick()
    is EditLoginAction -> state.loginsEditLoginState?.let {
        state.handleEditLoginAction(action)
    } ?: state
    is AddLoginAction -> state.loginsAddLoginState?.let {
        state.handleAddLoginAction(action)
    } ?: state.copy(
        loginsAddLoginState = LoginsAddLoginState(
            host = "",
            username = "",
            password = "",
        ),
        newLoginState = NewLoginState.None,
    )
    is DetailLoginAction -> state
    is DetailLoginMenuAction.EditLoginMenuItemClicked -> state.copy(
        loginsEditLoginState = LoginsEditLoginState(
            login = action.item,
            newUsername = action.item.username,
            newPassword = action.item.password,
            isPasswordVisible = true,
        ),
    )
    is DetailLoginMenuAction.DeleteLoginMenuItemClicked -> state.copy(
        loginsDeletionState = state.loginsLoginDetailState?.let {
            LoginDeletionState.Presenting(it.login.guid)
        },
        loginsLoginDetailState = null,
    )
    is LoginsListBackClicked -> state.respondToLoginsListBackClick()
    is AddLoginBackClicked -> state.respondToAddLoginBackClick()
    is EditLoginBackClicked -> state.respondToEditLoginBackClick()
    ViewDisposed,
    is Init, LearnMoreAboutSync,
    -> state
}

private fun LoginsState.handleSearchLogins(action: SearchLogins): LoginsState = copy(
    searchText = action.searchText,
    loginItems = action.loginItems.filter {
        it.url.contains(
            action.searchText,
            ignoreCase = true,
        )
    },
)

private fun LoginsState.handleLoginsLoadedAction(action: LoginsLoaded): LoginsState =
    copy(
        loginItems = if (searchText.isNullOrEmpty()) {
            action.loginItems.sortedWith(sortOrder.comparator)
        } else {
            action.loginItems.sortedWith(sortOrder.comparator)
                .filter { it.url.contains(searchText, ignoreCase = true) }
        },
    )

private fun LoginsState.respondToLoginsListBackClick(): LoginsState = when {
    loginsListState != null -> copy(loginsListState = null)
    else -> this
}

private fun LoginsState.handleSortMenuAction(action: LoginsListSortMenuAction): LoginsState =
    when (action) {
        LoginsListSortMenuAction.OrderByLastUsedClicked -> copy(sortOrder = LoginsSortOrder.LastUsed)
        LoginsListSortMenuAction.OrderByNameClicked -> copy(sortOrder = LoginsSortOrder.Alphabetical)
    }.let {
        it.copy(
            loginItems = it.loginItems.sortedWith(it.sortOrder.comparator),
        )
    }

private fun LoginsState.handleEditLoginAction(action: EditLoginAction): LoginsState =
    when (action) {
        is EditLoginAction.UsernameChanged -> copy(
            loginsEditLoginState = this.loginsEditLoginState?.copy(newUsername = action.usernameChanged),
        )
        is EditLoginAction.PasswordChanged -> copy(
            loginsEditLoginState = this.loginsEditLoginState?.copy(newPassword = action.passwordChanged),
        )
        is EditLoginAction.PasswordVisibilityChanged -> copy(
            loginsEditLoginState = this.loginsEditLoginState?.copy(isPasswordVisible = action.isPasswordVisible),
        )
        else -> this
    }

private fun LoginsState.handleAddLoginAction(action: AddLoginAction): LoginsState =
    when (action) {
        is AddLoginAction.InitAdd -> copy(
            loginsAddLoginState = LoginsAddLoginState(
                host = "",
                username = "",
                password = "",
            ),
        )
        is AddLoginAction.AddLoginSaveClicked -> copy(
            newLoginState = NewLoginState.None,
        )
        is AddLoginAction.HostChanged -> copy(
            loginsAddLoginState = this.loginsAddLoginState?.copy(
                host = action.hostChanged,
            ),
            newLoginState = if (loginItems.hasDuplicate(
                    action.hostChanged,
                    this.loginsAddLoginState?.username,
                )
            ) {
                NewLoginState.Duplicate
            } else {
                NewLoginState.None
            },
        )
        is AddLoginAction.UsernameChanged -> copy(
            loginsAddLoginState = this.loginsAddLoginState?.copy(
                username = action.usernameChanged,
            ),
            newLoginState = if (loginItems.hasDuplicate(
                    this.loginsAddLoginState?.host,
                    action.usernameChanged,
                )
            ) {
                NewLoginState.Duplicate
            } else {
                NewLoginState.None
            },
        )
        is AddLoginAction.PasswordChanged -> copy(
            loginsAddLoginState = this.loginsAddLoginState?.copy(
                password = action.passwordChanged,
            ),
        )
    }

private fun LoginsState.respondToLoginsDetailBackClick(): LoginsState = when {
    loginsLoginDetailState != null -> copy(
        loginsLoginDetailState = null,
        loginsDeletionState = null,
    )

    else -> this
}

private fun LoginsState.respondToAddLoginBackClick(): LoginsState = when {
    loginsAddLoginState != null -> copy(
        newLoginState = null,
        loginsAddLoginState = null,
    )

    else -> this
}

private fun LoginsState.respondToEditLoginBackClick(): LoginsState = when {
    loginsEditLoginState != null -> copy(
        loginsEditLoginState = null,
    )

    else -> this
}

private fun List<LoginItem>.hasDuplicate(host: String?, username: String?): Boolean =
    this.isNotEmpty() && this.any { it.url == host && it.username == username }
