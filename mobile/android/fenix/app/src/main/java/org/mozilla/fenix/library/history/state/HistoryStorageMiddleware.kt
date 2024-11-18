/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore

/**
 * A [Middleware] for initiating storage side-effects based on [HistoryFragmentAction]s that are
 * dispatched to the [HistoryFragmentStore].
 *
 * @param browserStore To dispatch Actions to update global state.
 * @param historyStorage To update storage as a result of some Actions.
 * @param onTimeFrameDeleted Called when a time range of items is successfully deleted.
 * @param scope CoroutineScope to launch storage operations into.
 */
class HistoryStorageMiddleware(
    private val browserStore: BrowserStore,
    private val historyStorage: PlacesHistoryStorage,
    private val onTimeFrameDeleted: () -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<HistoryFragmentState, HistoryFragmentAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryFragmentState, HistoryFragmentAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        next(action)
        when (action) {
            is HistoryFragmentAction.DeleteTimeRange -> {
                context.store.dispatch(HistoryFragmentAction.EnterDeletionMode)
                scope.launch {
                    if (action.timeFrame == null) {
                        historyStorage.deleteEverything()
                    } else {
                        val longRange = action.timeFrame.toLongRange()
                        historyStorage.deleteVisitsBetween(
                            startTime = longRange.first,
                            endTime = longRange.last,
                        )
                    }
                    browserStore.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction)
                    browserStore.dispatch(EngineAction.PurgeHistoryAction).join()

                    context.store.dispatch(HistoryFragmentAction.ExitDeletionMode)
                    launch(Dispatchers.Main) {
                        onTimeFrameDeleted()
                    }
                }
            }
            else -> Unit
        }
    }
}
