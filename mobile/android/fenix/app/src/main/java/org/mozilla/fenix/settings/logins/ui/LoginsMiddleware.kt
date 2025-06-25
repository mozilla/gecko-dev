/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.settings.SupportUtils

/**
 * A middleware for handling side-effects in response to [LoginsAction]s.
 *
 * @param loginsStorage Storage layer for reading and writing logins.
 * @param getNavController Fetch the NavController for navigating within the local Composable nav graph.
 * @param exitLogins Invoked when back is clicked while the navController's backstack is empty.
 * @param persistLoginsSortOrder Invoked to persist the new sorting order for logins.
 * @param openTab Invoked when opening a tab when a login url is clicked.
 * @param ioDispatcher Coroutine dispatcher for IO operations.
 */
@Suppress("LongParameterList")
internal class LoginsMiddleware(
    private val loginsStorage: LoginsStorage,
    private val getNavController: () -> NavController,
    private val exitLogins: () -> Unit,
    private val persistLoginsSortOrder: suspend (LoginsSortOrder) -> Unit,
    private val openTab: (url: String, openInNewTab: Boolean) -> Unit,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : Middleware<LoginsState, LoginsAction> {

    private val scope = CoroutineScope(ioDispatcher)

    @Suppress("LongMethod", "ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<LoginsState, LoginsAction>,
        next: (LoginsAction) -> Unit,
        action: LoginsAction,
    ) {
        next(action)

        when (action) {
            Init -> {
                context.store.loadLoginsList()
            }
            is InitEdit -> scope.launch {
                Result.runCatching {
                    val login = loginsStorage.get(action.guid)
                    val loginItem = login?.let {
                        LoginItem(
                            guid = it.guid,
                            url = it.formActionOrigin ?: "",
                            username = it.username,
                            password = it.password,
                        )
                    }
                    InitEditLoaded(login = loginItem!!)
                }.getOrNull()?.also {
                    context.store.dispatch(it)
                }
            }
            is InitAdd -> {
                getNavController().navigate(LoginsDestinations.ADD_LOGIN)
            }
            is LoginClicked -> {
                getNavController().navigate(LoginsDestinations.LOGIN_DETAILS)
            }
            is SearchLogins -> {
                context.store.loadLoginsList()
            }
            is LoginsListBackClicked -> exitLogins()
            is DetailLoginMenuAction.EditLoginMenuItemClicked -> getNavController().navigate(
                LoginsDestinations.EDIT_LOGIN,
            )
            is DetailLoginMenuAction.DeleteLoginMenuItemClicked -> {
                scope.launch {
                    loginsStorage.delete(action.item.guid)
                }
            }
            is LoginsListSortMenuAction -> scope.launch {
                persistLoginsSortOrder(context.store.state.sortOrder)
            }
            is LearnMoreAboutSync -> {
                openTab(
                    SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.SYNC_SETUP),
                    true,
                )
            }
            is DetailLoginAction.GoToSiteClicked -> {
                openTab(action.url, true)
            }
            is InitEditLoaded,
            is EditLoginAction.UsernameChanged,
            is AddLoginAction.BackAddClicked,
            is DetailLoginAction.BackDetailClicked,
            is EditLoginAction.BackEditClicked,
            is DetailLoginAction.CopyPasswordClicked,
            is DetailLoginAction.CopyUsernameClicked,
            is InitAddLoaded,
            is InitDetails,
            is LoginsLoaded,
            is DetailLoginAction.OptionsMenuClicked,
            is EditLoginAction.PasswordChanged,
            is AddLoginAction.PasswordChanged,
            is EditLoginAction.PasswordClearClicked,
            is AddLoginAction.PasswordClearClicked,
            is EditLoginAction.PasswordVisible,
            is DetailLoginAction.PasswordVisibleClicked,
            is AddLoginAction.SaveAddClicked,
            is EditLoginAction.SaveEditClicked,
            is AddLoginAction.UrlChanged,
            is AddLoginAction.UrlClearClicked,
            is AddLoginAction.UsernameChanged,
            is EditLoginAction.UsernameClearClicked,
            is AddLoginAction.UsernameClearClicked,
            is ViewDisposed,
            -> Unit
        }
    }

    private fun Store<LoginsState, LoginsAction>.loadLoginsList() = scope.launch {
        val loginItems = arrayListOf<LoginItem>()
        val items = loginsStorage.list()
        items.forEach { login ->
            loginItems.add(
                LoginItem(
                    guid = login.guid,
                    url = login.origin,
                    username = login.username,
                    password = login.password,
                    timeLastUsed = login.timeLastUsed,
                ),
            )
        }

        dispatch(LoginsLoaded(loginItems))
    }
}
