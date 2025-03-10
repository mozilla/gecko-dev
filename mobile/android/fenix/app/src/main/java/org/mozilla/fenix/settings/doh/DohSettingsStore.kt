/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import androidx.navigation.NavController
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Reducer
import mozilla.components.lib.state.UiStore
import org.mozilla.fenix.HomeActivity
import org.webrtc.EglBase.Context

/**
 * A helper class to be able to change the reference to objects that get replaced when the activity
 * gets recreated.
 *
 * @property context An Android [Context].
 * @property navController A [NavController] for interacting with the androidx navigation library.
 * @property composeNavController A [NavController] for navigating within the local Composable nav graph.
 * @property settingsProvider A [DefaultDohSettingsProvider] for connecting DoH modes/providers via GeckoView API.
 * @property homeActivity The [HomeActivity] that provides a reference to the browsing mode and allows
 *   for opening a URL in the browser.
 */
internal class LifecycleHolder(
    var context: android.content.Context,
    var navController: NavController,
    var composeNavController: NavController,
    var settingsProvider: DefaultDohSettingsProvider,
    var homeActivity: HomeActivity,
)

/**
 * A Store for handling [DohSettingsState] and dispatching [DohSettingsAction].
 *
 * @param initialState The initial state for the Store.
 * @param reducer Reducer to handle state updates based on dispatched actions.
 * @param middleware Middleware to handle side-effects in response to dispatched actions.
 * @property lifecycleHolder a hack to box the references to objects that get recreated with the activity.
 */
internal class DohSettingsStore(
    initialState: DohSettingsState = DohSettingsState(),
    reducer: Reducer<DohSettingsState, DohSettingsAction> = ::dohSettingsReducer,
    middleware: List<Middleware<DohSettingsState, DohSettingsAction>> = listOf(),
    var lifecycleHolder: LifecycleHolder? = null,
) : UiStore<DohSettingsState, DohSettingsAction>(
    initialState = initialState,
    reducer = reducer,
    middleware = middleware,
) {
    init {
        dispatch(Init)
    }
}
