/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import androidx.compose.runtime.Composable
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.components.appstate.AppState

/**
 * State object encapsulating the UI state of the collections section of the homepage.
 */
sealed class CollectionsState {

    /**
     * State in which a user has collections, and they should be displayed in the UI.
     *
     * @property collections List of [TabCollection] to display.
     * @property expandedCollections List of ids corresponding to [TabCollection]s which are currently expanded.
     * @property showAddTabToCollection  Whether to show the "Add tab" menu item in the collections menu.
     */
    data class Content(
        val collections: List<TabCollection>,
        val expandedCollections: Set<Long>,
        val showAddTabToCollection: Boolean,
    ) : CollectionsState()

    /**
     * State in which the placeholder should be displayed.
     */
    data object Placeholder : CollectionsState()

    /**
     * State in which no collections section should be displayed.
     */
    data object Gone : CollectionsState()

    companion object {
        @Composable
        internal fun build(appState: AppState, browserState: BrowserState): CollectionsState =
            with(appState) {
                when {
                    collections.isNotEmpty() -> Content(
                        collections = collections,
                        expandedCollections = expandedCollections,
                        showAddTabToCollection = browserState.normalTabs.isNotEmpty(),
                    )

                    showCollectionPlaceholder -> Placeholder
                    else -> Gone
                }
            }
    }
}
