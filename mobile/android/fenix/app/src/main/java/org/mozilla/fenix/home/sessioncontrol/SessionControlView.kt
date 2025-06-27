/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.service.nimbus.messaging.Message
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.ext.showWallpaperOnboardingDialog
import org.mozilla.fenix.messaging.FenixMessageSurfaceId

// This method got a little complex with the addition of the tab tray feature flag
// When we remove the tabs from the home screen this will get much simpler again.
@Suppress("ComplexMethod", "LongParameterList")
@VisibleForTesting
internal fun normalModeAdapterItems(
    collections: List<TabCollection>,
    expandedCollections: Set<Long>,
    nimbusMessageCard: Message? = null,
): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()

    nimbusMessageCard?.let {
        items.add(AdapterItem.NimbusMessageCard(it))
    }

    if (!collections.isEmpty()) {
        showCollections(collections, expandedCollections, items)
    }

    return items
}

private fun showCollections(
    collections: List<TabCollection>,
    expandedCollections: Set<Long>,
    items: MutableList<AdapterItem>,
) {
    // If the collection is expanded, we want to add all of its tabs beneath it in the adapter
    collections.map {
        AdapterItem.CollectionItem(it, expandedCollections.contains(it.id))
    }.forEach {
        items.add(it)
        if (it.expanded) {
            items.addAll(collectionTabItems(it.collection))
        }
    }
}

private fun AppState.toAdapterList(): List<AdapterItem> =
    normalModeAdapterItems(
        collections,
        expandedCollections,
        messaging.messageToShow[FenixMessageSurfaceId.HOMESCREEN],
    )

private fun collectionTabItems(collection: TabCollection) =
    collection.tabs.mapIndexed { index, tab ->
        AdapterItem.TabInCollectionItem(collection, tab, index == collection.tabs.lastIndex)
    }

/**
 * Shows a list of Home screen views.
 *
 * @param containerView The [View] that is used to initialize the Home recycler view.
 * @param viewLifecycleOwner [LifecycleOwner] for the view.
 * @param interactor [SessionControlInteractor] which will have delegated to all user interactions.
 */
class SessionControlView(
    containerView: View,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: SessionControlInteractor,
) {

    val view: RecyclerView = containerView as RecyclerView

    private val sessionControlAdapter = SessionControlAdapter(
        interactor,
        viewLifecycleOwner,
    )

    init {
        @Suppress("NestedBlockDepth")
        view.apply {
            adapter = sessionControlAdapter
            layoutManager = object : LinearLayoutManager(containerView.context) {
                override fun onLayoutCompleted(state: RecyclerView.State?) {
                    super.onLayoutCompleted(state)

                    if (settings().showWallpaperOnboardingDialog()) {
                        interactor.showWallpapersOnboardingDialog(
                            context.components.appStore.state.wallpaperState,
                        )
                    }

                    // We want some parts of the home screen UI to be rendered first if they are
                    // the most prominent parts of the visible part of the screen.
                    // For this reason, we wait for the home screen recycler view to finish it's
                    // layout and post an update for when it's best for non-visible parts of the
                    // home screen to render itself.
                    containerView.context.components.appStore.dispatch(
                        AppAction.UpdateFirstFrameDrawn(true),
                    )
                }
            }
        }
    }

    fun update(state: AppState, shouldReportMetrics: Boolean = false) {
        if (shouldReportMetrics) interactor.reportSessionMetrics(state)

        sessionControlAdapter.submitList(state.toAdapterList())
    }
}
