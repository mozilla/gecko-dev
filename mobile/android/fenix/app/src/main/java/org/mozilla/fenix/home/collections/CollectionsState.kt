/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import androidx.compose.runtime.Composable
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
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
     * @property showSaveTabsToCollection Whether to show the "Save tabs to collection" menu item in the collections
     * menu.
     */
    data class Content(
        val collections: List<TabCollection>,
        val expandedCollections: Set<Long>,
        val showSaveTabsToCollection: Boolean,
    ) : CollectionsState()

    /**
     * State in which the placeholder should be displayed.
     *
     * @property showSaveTabsToCollection Whether to show the "Save tabs to collection" menu item in the collections
     * menu.
     */
    data class Placeholder(
        val showSaveTabsToCollection: Boolean,
    ) : CollectionsState()

    /**
     * State in which no collections section should be displayed.
     */
    data object Gone : CollectionsState()

    companion object {
        @Composable
        internal fun build(
            appState: AppState,
            browserState: BrowserState,
            browsingModeManager: BrowsingModeManager,
        ): CollectionsState =
            with(appState) {
                when {
                    collections.isNotEmpty() -> Content(
                        collections = collections,
                        expandedCollections = expandedCollections,
                        showSaveTabsToCollection = browserState.normalTabs.isNotEmpty(),
                    )

                    showCollectionPlaceholder -> {
                        val tabCount = if (browsingModeManager.mode.isPrivate) {
                            browserState.privateTabs.size
                        } else {
                            browserState.normalTabs.size
                        }

                        Placeholder(
                            showSaveTabsToCollection = tabCount > 0,
                        )
                    }

                    else -> Gone
                }
            }
    }
}
