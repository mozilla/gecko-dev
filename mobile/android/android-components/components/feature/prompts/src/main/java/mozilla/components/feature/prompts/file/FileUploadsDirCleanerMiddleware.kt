/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import androidx.core.net.toUri
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] that observe when a user navigates away from a site and clean up,
 * temporary file uploads.
 */
class FileUploadsDirCleanerMiddleware(
    private val fileUploadsDirCleaner: FileUploadsDirCleaner,
) : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is ContentAction.UpdateUrlAction -> {
                context.state.findTab(action.sessionId)?.let { actualSession ->
                    val actualHost = actualSession.content.url.toUri().host
                    val newHost = action.url.toUri().host
                    if (actualHost != newHost) {
                        fileUploadsDirCleaner.cleanRecentUploads()
                    }
                }
            }
            else -> {
                // no-op
            }
        }
        next(action)
    }
}
