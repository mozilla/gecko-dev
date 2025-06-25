/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.launch
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.Init
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import mozilla.components.lib.state.Action as MVIAction

/**
 * [Middleware] for synchronizing whether a search is active between [BrowserToolbarStore] and [AppStore].
 *
 * @param appStore [AppStore] through which the toolbar updates can be integrated with other application features.
 */
class BrowserToolbarSearchStatusSyncMiddleware(
    private val appStore: AppStore,
) : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private var store: BrowserToolbarStore? = null
    private lateinit var dependencies: LifecycleDependencies
    private var syncSearchActiveJob: Job? = null

    /**
     * Updates the lifecycle [LifecycleDependencies].
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
        if (syncSearchActiveJob != null) {
            syncSearchActive()
        }
    }

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        next(action)

        if (action is Init) {
            store = context.store as BrowserToolbarStore
        }

        if (action is ToggleEditMode) {
            when (action.editMode) {
                true -> syncSearchActive()
                false -> syncSearchActiveJob?.cancel()
            }
            appStore.dispatch(UpdateSearchBeingActiveState(isSearchActive = action.editMode))
        }
    }

    private fun syncSearchActive() {
        syncSearchActiveJob?.cancel()
        syncSearchActiveJob = observeWhileActive(appStore) {
            distinctUntilChangedBy { it.isSearchActive }
                .collect {
                    if (!it.isSearchActive) {
                        store?.dispatch(ToggleEditMode(false))
                    }
                }
        }
    }

    private inline fun <S : State, A : MVIAction> observeWhileActive(
        store: Store<S, A>,
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job = with(dependencies.lifecycleOwner) {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                store.flow().observe()
            }
        }
    }

    /**
     * Lifecycle dependencies for the [BrowserToolbarSearchStatusSyncMiddleware].
     *
     * @property lifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     */
    data class LifecycleDependencies(
        val lifecycleOwner: LifecycleOwner,
    )

    /**
     * Static functionalities of the [BrowserToolbarSearchStatusSyncMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [BrowserToolbarSearchStatusSyncMiddleware].
         *
         * @param appStore The [AppStore] to sync from.
         */
        fun viewModelFactory(
            appStore: AppStore,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                (BrowserToolbarSearchStatusSyncMiddleware(appStore) as? T)
                    ?: throw IllegalArgumentException("Unknown ViewModel class")
        }
    }
}
