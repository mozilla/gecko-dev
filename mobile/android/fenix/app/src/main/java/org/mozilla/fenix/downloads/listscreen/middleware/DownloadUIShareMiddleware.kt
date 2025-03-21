/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import android.content.Context
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.ktx.android.content.share
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState

/**
 * Middleware for sharing the Download item's URL.
 *
 * @param applicationContext A [Context] used to share the URL.
 */
class DownloadUIShareMiddleware(
    private val applicationContext: Context,
) : Middleware<DownloadUIState, DownloadUIAction> {

    override fun invoke(
        context: MiddlewareContext<DownloadUIState, DownloadUIAction>,
        next: (DownloadUIAction) -> Unit,
        action: DownloadUIAction,
    ) {
        next(action)
        when (action) {
            is DownloadUIAction.ShareUrlClicked -> applicationContext.share(action.item.url)
            else -> {
                // no - op
            }
        }
    }
}
