/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

private const val PAGE_FULLY_LOADED_PROGRESS = 100

/**
 * Middleware that checks if the current page is a PDF and dispatches [ContentAction.EnteredPdfViewer]
 * with the result.
 *
 * @param scope [CoroutineScope] used for long running tasks.
 */
internal class PdfStateMiddleware(
    private val scope: CoroutineScope,
) : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        next(action)

        if (action is ContentAction.UpdateProgressAction && action.progress == PAGE_FULLY_LOADED_PROGRESS) {
            scope.launch {
                val newPdfRenderingStatus = isRenderingPdf(action.sessionId, context.state)
                val previousRenderingStatus = previousPdfRenderingStatus(action.sessionId, context.state)

                if (newPdfRenderingStatus != previousRenderingStatus) {
                    dispatchPdfStatusUpdate(action.sessionId, newPdfRenderingStatus, context)
                }
            }
        }
    }

    private fun dispatchPdfStatusUpdate(
        sessionId: String,
        isPdf: Boolean,
        context: MiddlewareContext<BrowserState, BrowserAction>,
    ) = context.store.dispatch(
        when (isPdf) {
            true -> ContentAction.EnteredPdfViewer(sessionId)
            false -> ContentAction.ExitedPdfViewer(sessionId)
        },
    )

    private fun previousPdfRenderingStatus(sessionId: String, state: BrowserState): Boolean {
        return state.findTabOrCustomTabOrSelectedTab(sessionId)?.content?.isPdf ?: false
    }

    private suspend fun isRenderingPdf(sessionId: String, state: BrowserState): Boolean {
        val tab = state.findTabOrCustomTabOrSelectedTab(sessionId) ?: return false
        val deferredResult = CompletableDeferred<Boolean>()

        tab.engineState.engineSession?.checkForPdfViewer(
            onResult = { isPdf -> deferredResult.complete(isPdf) },
            onException = { deferredResult.complete(false) },
        )

        return deferredResult.await()
    }
}
