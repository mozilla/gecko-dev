/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.search.SearchFragmentAction.SearchStarted

/**
 * [SearchFragmentStore] [Middleware] to synchronize search related details from [BrowserToolbarStore].
 *
 * @param toolbarStore The [BrowserToolbarStore] to sync from.
 */
class BrowserToolbarToFenixSearchMapperMiddleware(
    private val toolbarStore: BrowserToolbarStore,
) : Middleware<SearchFragmentState, SearchFragmentAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    private var store: SearchDialogFragmentStore? = null
    private var syncEditModeJob: Job? = null

    /**
     * Updates the lifecycle [LifecycleDependencies].
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
        syncEditModeJob?.cancel()
        store?.let { syncUserQuery(it) }
    }

    override fun invoke(
        context: MiddlewareContext<SearchFragmentState, SearchFragmentAction>,
        next: (SearchFragmentAction) -> Unit,
        action: SearchFragmentAction,
    ) {
        if (action is SearchStarted) {
            store = context.store as SearchDialogFragmentStore
            syncUserQuery(context.store)
        }
        next(action)
    }

    private fun syncUserQuery(store: Store<SearchFragmentState, SearchFragmentAction>) {
        syncEditModeJob = dependencies.lifecycleScope.launch {
            toolbarStore.flow()
                .map { it.editState.editText }
                .distinctUntilChanged()
                .collect { query ->
                    store.dispatch(SearchFragmentAction.UpdateQuery(query ?: ""))
                }
        }
    }

    /**
     * Lifecycle dependencies for the [BrowserToolbarToFenixSearchMapperMiddleware].
     *
     * @property lifecycleScope The [CoroutineScope] in which to listen for search updates.
     */
    data class LifecycleDependencies(
        val lifecycleScope: CoroutineScope,
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
