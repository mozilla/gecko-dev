/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.gleandebugtools

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.UiStore

/**
 * Value type that represents the state of the Glean Debug Tools.
 *
 * @property logPingsToConsoleEnabled Whether logging pings to console is enabled.
 * @property debugViewTag The debug view tag of the ping.
 */
data class GleanDebugToolsState(
    val logPingsToConsoleEnabled: Boolean,
    val debugViewTag: String,
) : State {

    /**
     * Check whether the debug view tag related buttons should be enabled, based on whether a debug
     * view tag is provided and whether it meets the criteria of being a debug view tag.
     */
    val isDebugTagButtonEnabled: Boolean
        get() = !hasDebugViewTagError && debugViewTag.isNotEmpty()

    val hasDebugViewTagError: Boolean
        get() = debugViewTag.length > DEBUG_VIEW_TAG_MAX_LENGTH

    companion object {
        internal const val DEBUG_VIEW_TAG_MAX_LENGTH = 20
    }
}

/**
 * [Action] implementation related to [GleanDebugToolsStore].
 */
sealed class GleanDebugToolsAction : Action {

    /**
     * Toggle whether to log pings to console.
     */
    data object LogPingsToConsoleToggled : GleanDebugToolsAction()

    /**
     * Change the debug view tag to [newTag].
     */
    data class DebugViewTagChanged(val newTag: String) : GleanDebugToolsAction()

    /**
     * Send a pending event ping.
     */
    data object SendPendingEventPing : GleanDebugToolsAction()

    /**
     * Send a baseline ping.
     */
    data object SendBaselinePing : GleanDebugToolsAction()

    /**
     * Send a metrics ping.
     */
    data object SendMetricsPing : GleanDebugToolsAction()

    /**
     * Open the relevant debug view.
     *
     * @property useDebugViewTag Whether the debug view tag should be used to open the debug view.
     */
    data class OpenDebugView(val useDebugViewTag: Boolean) : GleanDebugToolsAction()

    /**
     * Add the relevant debug view tag to clipboard.
     *
     * @property useDebugViewTag Whether to copy the debug view link with the debug view tag.
     */
    data class CopyDebugViewLink(val useDebugViewTag: Boolean) : GleanDebugToolsAction()
}

/**
 * Reducer for [GleanDebugToolsStore].
 */
internal object GleanDebugToolsReducer {
    fun reduce(state: GleanDebugToolsState, action: GleanDebugToolsAction): GleanDebugToolsState {
        return when (action) {
            is GleanDebugToolsAction.LogPingsToConsoleToggled ->
                state.copy(logPingsToConsoleEnabled = !state.logPingsToConsoleEnabled)
            is GleanDebugToolsAction.DebugViewTagChanged -> {
                state.copy(
                    debugViewTag = action.newTag,
                )
            }
            is GleanDebugToolsAction.SendBaselinePing -> state
            is GleanDebugToolsAction.SendMetricsPing -> state
            is GleanDebugToolsAction.SendPendingEventPing -> state
            is GleanDebugToolsAction.OpenDebugView -> state
            is GleanDebugToolsAction.CopyDebugViewLink -> state
        }
    }
}

/**
 * A [UiStore] that holds the [GleanDebugToolsState] for the Glean Debug Tools and reduces
 * [GleanDebugToolsAction]s dispatched to the store.
 */
class GleanDebugToolsStore(
    initialState: GleanDebugToolsState,
    middlewares: List<Middleware<GleanDebugToolsState, GleanDebugToolsAction>> = emptyList(),
) : UiStore<GleanDebugToolsState, GleanDebugToolsAction>(
    initialState,
    GleanDebugToolsReducer::reduce,
    middlewares,
)
