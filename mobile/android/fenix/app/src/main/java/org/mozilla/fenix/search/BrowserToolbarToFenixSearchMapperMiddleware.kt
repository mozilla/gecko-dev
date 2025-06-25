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
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.Mode
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.search.SearchFragmentAction.Init
import org.mozilla.fenix.search.SearchFragmentAction.SearchStarted
import mozilla.components.lib.state.Action as MVIAction

/**
 * [SearchFragmentStore] [Middleware] to synchronize search related details from [BrowserToolbarStore].
 *
 * @param toolbarStore The [BrowserToolbarStore] to sync from.
 */
class BrowserToolbarToFenixSearchMapperMiddleware(
    private val toolbarStore: BrowserToolbarStore,
) : Middleware<SearchFragmentState, SearchFragmentAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    private lateinit var searchStore: SearchFragmentStore
    private var syncSearchStartedJob: Job? = null
    private var syncSearchQueryJob: Job? = null
    private var isSyncingUserQueryInProgress = false

    /**
     * Updates the lifecycle [LifecycleDependencies].
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
        if (syncSearchStartedJob != null) {
            syncSearchStatus()
        }
        if (isSyncingUserQueryInProgress == true) {
            syncUserQuery()
        }
    }

    override fun invoke(
        context: MiddlewareContext<SearchFragmentState, SearchFragmentAction>,
        next: (SearchFragmentAction) -> Unit,
        action: SearchFragmentAction,
    ) {
        if (action is Init) {
            searchStore = context.store as SearchFragmentStore
            syncSearchStatus()
        }

        next(action)
    }

    private fun syncSearchStatus() {
        syncSearchStartedJob?.cancel()
        syncSearchStartedJob = observeWhileActive(toolbarStore) {
            distinctUntilChangedBy { it.mode }
                .collect {
                    if (it.mode == Mode.EDIT) {
                        searchStore.dispatch(
                            SearchStarted(
                                selectedSearchEngine = null,
                                inPrivateMode = dependencies.browsingModeManager.mode == BrowsingMode.Private,
                            ),
                        )

                        syncUserQuery()
                    } else {
                        stopSyncingUserQuery()
                    }
                }
        }
    }

    private fun syncUserQuery() {
        syncSearchQueryJob?.cancel()
        isSyncingUserQueryInProgress = true
        syncSearchQueryJob = observeWhileActive(toolbarStore) {
            map { it.editState.editText }
                .distinctUntilChanged()
                .collect { query ->
                    searchStore.dispatch(SearchFragmentAction.UpdateQuery(query ?: ""))
                }
        }
    }

    private fun stopSyncingUserQuery() {
        isSyncingUserQueryInProgress = false
        syncSearchQueryJob?.cancel()
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
     * Lifecycle dependencies for the [BrowserToolbarToFenixSearchMapperMiddleware].
     *
     * @property browsingModeManager The [BrowsingModeManager] to sync from.
     * @property lifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     */
    data class LifecycleDependencies(
        val browsingModeManager: BrowsingModeManager,
        val lifecycleOwner: LifecycleOwner,
    )

    /**
     * Static functionalities of the [BrowserToolbarToFenixSearchMapperMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [BrowserToolbarToFenixSearchMapperMiddleware].
         *
         * @param toolbarStore The [BrowserToolbarStore] to sync from.
         */
        fun viewModelFactory(
            toolbarStore: BrowserToolbarStore,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                (BrowserToolbarToFenixSearchMapperMiddleware(toolbarStore) as? T)
                    ?: throw IllegalArgumentException("Unknown ViewModel class")
        }
    }
}
