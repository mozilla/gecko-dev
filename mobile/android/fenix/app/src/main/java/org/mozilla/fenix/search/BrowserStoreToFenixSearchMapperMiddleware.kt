/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelProvider.Factory
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.search.SearchFragmentAction.Init
import org.mozilla.fenix.search.SearchFragmentAction.UpdateSearchState
import mozilla.components.lib.state.Action as MVIAction

/**
 * [SearchFragmentStore] [Middleware] to synchronize search related details from [BrowserStore].
 *
 * @param browserStore The [BrowserStore] to sync from.
 */
class BrowserStoreToFenixSearchMapperMiddleware(
    private val browserStore: BrowserStore,
) : Middleware<SearchFragmentState, SearchFragmentAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    private var searchStore: SearchFragmentStore? = null
    private var observeBrowserSearchStateJob: Job? = null

    /**
     * Updates the [LifecycleDependencies] of this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
        if (searchStore != null) {
            observeBrowserSearchState()
        }
    }

    override fun invoke(
        context: MiddlewareContext<SearchFragmentState, SearchFragmentAction>,
        next: (SearchFragmentAction) -> Unit,
        action: SearchFragmentAction,
    ) {
        if (action is Init) {
            searchStore = context.store as SearchFragmentStore

            observeBrowserSearchState()
        }

        next(action)
    }

    private fun observeBrowserSearchState() {
        observeBrowserSearchStateJob?.cancel()
        observeBrowserSearchStateJob = browserStore.observeWhileActive(dependencies.lifecycleOwner) {
            map { it.search }
                .distinctUntilChanged()
                .collect { searchState ->
                    searchStore?.dispatch(
                        UpdateSearchState(searchState, true),
                    )
                }
        }
    }

    private inline fun <S : State, A : MVIAction> Store<S, A>.observeWhileActive(
        lifecycleOwner: LifecycleOwner,
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job = with(lifecycleOwner) {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                flow().observe()
            }
        }
    }

    /**
     * Lifecycle dependencies for the [BrowserStoreToFenixSearchMapperMiddleware].
     *
     * @property lifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     */
    data class LifecycleDependencies(
        val lifecycleOwner: LifecycleOwner,
    )

    /**
     * Static functionalities of the [BrowserStoreToFenixSearchMapperMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [BrowserStoreToFenixSearchMapperMiddleware].
         *
         * @param browserStore The [BrowserStore] to sync from.
         */
        fun viewModelFactory(
            browserStore: BrowserStore,
        ) = object : Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                (BrowserStoreToFenixSearchMapperMiddleware(browserStore) as? T)
                    ?: throw IllegalArgumentException("Unknown ViewModel class")
        }
    }
}
