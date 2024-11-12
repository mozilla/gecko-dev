/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore

/**
 * A [Middleware] for initiating navigation events based on [HistoryFragmentAction]s that are
 * dispatched to the [HistoryFragmentStore].
 *
 * @param onBackPressed Callback to handle back press actions.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
class HistoryNavigationMiddleware(
    private val onBackPressed: () -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<HistoryFragmentState, HistoryFragmentAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryFragmentState, HistoryFragmentAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        // Read the current state before letting the chain process the action, so that clicks are
        // treated correctly in reference to the number of selected items.
        val currentState = context.state
        next(action)
        scope.launch {
            when (action) {
                is HistoryFragmentAction.BackPressed -> {
                    // When editing, we override the back pressed event to update the mode.
                    if (currentState.mode !is HistoryFragmentState.Mode.Editing) {
                        onBackPressed()
                    }
                }
                else -> Unit
            }
        }
    }
}
