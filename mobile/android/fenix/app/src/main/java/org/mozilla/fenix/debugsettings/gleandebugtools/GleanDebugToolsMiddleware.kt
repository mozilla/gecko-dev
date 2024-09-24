/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.gleandebugtools

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.ktx.kotlin.urlEncode
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.ClipboardHandler

internal const val PING_PREVIEW_URL = "https://debug-ping-preview.firebaseapp.com/"

/**
 * [Middleware] that reacts to various [GleanDebugToolsAction]s.
 *
 * @param gleanDebugToolsService [GleanDebugToolsService] used to dispatch calls to the Glean API.
 * @param clipboardHandler [ClipboardHandler] used to add the debug view link to the clipboard.
 * @param openDebugView Invoked when the user clicks on the open debug view button.
 * @param showToast Invoked when the user sends a test ping.
 */
class GleanDebugToolsMiddleware(
    private val gleanDebugToolsService: GleanDebugToolsService,
    private val clipboardHandler: ClipboardHandler,
    private val openDebugView: (String) -> Unit,
    private val showToast: (Int) -> Unit,
) : Middleware<GleanDebugToolsState, GleanDebugToolsAction> {
    override fun invoke(
        context: MiddlewareContext<GleanDebugToolsState, GleanDebugToolsAction>,
        next: (GleanDebugToolsAction) -> Unit,
        action: GleanDebugToolsAction,
    ) {
        next(action)
        when (action) {
            is GleanDebugToolsAction.LogPingsToConsoleToggled -> {
                gleanDebugToolsService.setLogPings(context.state.logPingsToConsoleEnabled)
            }
            is GleanDebugToolsAction.OpenDebugView -> {
                val debugViewLink = getDebugViewLink(
                    debugViewTag = context.state.debugViewTag,
                    useDebugViewTag = action.useDebugViewTag,
                )
                openDebugView(debugViewLink)
            }
            is GleanDebugToolsAction.CopyDebugViewLink -> {
                val debugViewLink = getDebugViewLink(
                    debugViewTag = context.state.debugViewTag,
                    useDebugViewTag = action.useDebugViewTag,
                )
                clipboardHandler.text = debugViewLink
            }
            is GleanDebugToolsAction.DebugViewTagChanged -> {} // No-op
            is GleanDebugToolsAction.SendBaselinePing -> {
                gleanDebugToolsService.sendBaselinePing(
                    debugViewTag = context.state.debugViewTag,
                )
                showToast(R.string.glean_debug_tools_send_baseline_ping_toast_message)
            }
            is GleanDebugToolsAction.SendMetricsPing -> {
                gleanDebugToolsService.sendMetricsPing(
                    debugViewTag = context.state.debugViewTag,
                )
                showToast(R.string.glean_debug_tools_send_metrics_ping_toast_message)
            }
            is GleanDebugToolsAction.SendPendingEventPing -> {
                gleanDebugToolsService.sendPendingEventPing(
                    debugViewTag = context.state.debugViewTag,
                )
                showToast(R.string.glean_debug_tools_send_pending_event_ping_toast_message)
            }
        }
    }

    /**
     * Get the debug view link for the given debug view tag.
     *
     * @param debugViewTag The debug view tag to use to get the link to the debug view.
     * @param useDebugViewTag Whether to use the given debug view tag to get the debug view link.
     */
    private fun getDebugViewLink(debugViewTag: String, useDebugViewTag: Boolean): String =
        if (useDebugViewTag) {
            "${PING_PREVIEW_URL}pings/${debugViewTag.urlEncode()}"
        } else {
            PING_PREVIEW_URL
        }
}
