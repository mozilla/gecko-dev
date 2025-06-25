/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Reducer
import mozilla.components.lib.state.UiStore
import org.mozilla.fenix.lifecycle.LifecycleHolder

/**
 * A Store for handling [LoginsState] and dispatching [LoginsAction].
 *
 * @param initialState The initial state for the Store.
 * @param reducer Reducer to handle state updates based on dispatched actions.
 * @param middleware Middleware to handle side-effects in response to dispatched actions.
 * @property lifecycleHolder a hack to box the references to objects that get recreated with the activity.
 * @param loginToLoad The guid of a login to load when landing on the edit/details screen.
 */
internal class LoginsStore(
    initialState: LoginsState = LoginsState(),
    reducer: Reducer<LoginsState, LoginsAction> = ::loginsReducer,
    middleware: List<Middleware<LoginsState, LoginsAction>> = listOf(),
    val lifecycleHolder: LifecycleHolder? = null,
    loginToLoad: String? = null,
) : UiStore<LoginsState, LoginsAction>(
    initialState = initialState,
    reducer = reducer,
    middleware = middleware,
) {
    init {
        val action = loginToLoad?.let { InitEdit(loginToLoad) } ?: Init
        dispatch(action)
    }
}
