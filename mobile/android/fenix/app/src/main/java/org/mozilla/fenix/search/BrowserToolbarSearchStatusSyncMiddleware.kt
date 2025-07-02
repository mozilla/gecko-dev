/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.launch
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.EnvironmentCleared
import mozilla.components.compose.browser.toolbar.store.EnvironmentRehydrated
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import org.mozilla.fenix.home.toolbar.HomeToolbarEnvironment
import mozilla.components.lib.state.Action as MVIAction

/**
 * [Middleware] for synchronizing whether a search is active between [BrowserToolbarStore] and [AppStore].
 *
 * @param appStore [AppStore] through which the toolbar updates can be integrated with other application features.
 */
class BrowserToolbarSearchStatusSyncMiddleware(
    private val appStore: AppStore,
) : Middleware<BrowserToolbarState, BrowserToolbarAction> {
    @VisibleForTesting
    internal var environment: HomeToolbarEnvironment? = null
    private var syncSearchActiveJob: Job? = null

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        next(action)

        if (action is EnvironmentRehydrated) {
            environment = action.environment as? HomeToolbarEnvironment

            if (appStore.state.isSearchActive) {
                syncSearchActive(context.store)
            }
        }
        if (action is EnvironmentCleared) {
            environment = null
        }

        if (action is ToggleEditMode) {
            when (action.editMode) {
                true -> syncSearchActive(context.store)
                false -> syncSearchActiveJob?.cancel()
            }
            appStore.dispatch(UpdateSearchBeingActiveState(isSearchActive = action.editMode))
        }
    }

    private fun syncSearchActive(store: Store<BrowserToolbarState, BrowserToolbarAction>) {
        syncSearchActiveJob?.cancel()
        syncSearchActiveJob = appStore.observeWhileActive {
            distinctUntilChangedBy { it.isSearchActive }
                .collect {
                    if (!it.isSearchActive) {
                        store.dispatch(ToggleEditMode(false))
                    }
                }
        }
    }

    private inline fun <S : State, A : MVIAction> Store<S, A>.observeWhileActive(
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job? = environment?.viewLifecycleOwner?.run {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                flow().observe()
            }
        }
    }
}
