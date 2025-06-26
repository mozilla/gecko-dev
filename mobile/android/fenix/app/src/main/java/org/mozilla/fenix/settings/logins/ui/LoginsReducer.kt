/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

/**
 * Function for reducing a new logins state based on the received action.
 */
internal fun loginsReducer(state: LoginsState, action: LoginsAction) = when (action) {
    is InitEditLoaded -> state.copy(
        loginsEditLoginState = LoginsEditLoginState(
            login = action.login,
        ),
    )
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
        state.copy(loginsEditLoginState = it.handleEditLoginAction(action))
    } ?: state
    is AddLoginAction -> state.loginsAddLoginState?.let {
        state.copy(loginsAddLoginState = handleAddLoginAction(action))
    } ?: state
    is DetailLoginAction -> state
    is DetailLoginMenuAction.EditLoginMenuItemClicked -> state
    is DetailLoginMenuAction.DeleteLoginMenuItemClicked -> state.copy(
        loginsDeletionState = state.loginsLoginDetailState?.let {
            LoginDeletionState.Presenting(it.login.guid)
        },
        loginsLoginDetailState = null,
    )
    is LoginsListBackClicked -> state.respondToLoginsListBackClick()
    ViewDisposed,
    is InitEdit, Init, InitAdd, LearnMoreAboutSync, is InitAddLoaded,
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

private fun LoginsEditLoginState.handleEditLoginAction(action: EditLoginAction): LoginsEditLoginState? =
    when (action) {
        is EditLoginAction.UsernameChanged -> copy(
            login = login.copy(password = action.usernameChanged),
        )
        is EditLoginAction.PasswordChanged -> copy(
            login = login.copy(password = action.passwordChanged),
        )
        is EditLoginAction.UsernameClearClicked,
        is EditLoginAction.PasswordClearClicked,
        is EditLoginAction.PasswordVisible,
        is EditLoginAction.SaveEditClicked,
        is EditLoginAction.BackEditClicked,
        -> null
    }

private fun handleAddLoginAction(action: AddLoginAction): LoginsAddLoginState? =
    when (action) {
        is AddLoginAction.UrlChanged,
        is AddLoginAction.PasswordChanged,
        is AddLoginAction.UsernameChanged,
        is AddLoginAction.UrlClearClicked,
        is AddLoginAction.PasswordClearClicked,
        is AddLoginAction.UsernameClearClicked,
        is AddLoginAction.SaveAddClicked,
        is AddLoginAction.BackAddClicked,
        -> null
    }

private fun LoginsState.respondToLoginsDetailBackClick(): LoginsState = when {
    loginsLoginDetailState != null -> copy(
        loginsLoginDetailState = null,
        loginsDeletionState = null,
    )

    else -> this
}
