/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState

/**
 * Middleware responsible for handling actions related to download management.
 * It intercepts specific [DownloadUIAction]s and translates them into
 * broadcasts that are compatible with the [AbstractFetchDownloadService].
 *
 * @param broadcastSender An abstraction for sending broadcasts, allowing for easier
 * testing and potentially different broadcast mechanisms.
 */
class DownloadsServiceCommunicationMiddleware(
    private val broadcastSender: BroadcastSender,
) : Middleware<DownloadUIState, DownloadUIAction> {

    override fun invoke(
        context: MiddlewareContext<DownloadUIState, DownloadUIAction>,
        next: (DownloadUIAction) -> Unit,
        action: DownloadUIAction,
    ) {
        next(action)
        when (action) {
            is DownloadUIAction.PauseDownload ->
                broadcastSender.sendBroadcast(action.downloadId, AbstractFetchDownloadService.ACTION_PAUSE)

            is DownloadUIAction.ResumeDownload -> {
                broadcastSender.sendBroadcast(action.downloadId, AbstractFetchDownloadService.ACTION_RESUME)
            }

            is DownloadUIAction.CancelDownload -> {
                broadcastSender.sendBroadcast(action.downloadId, AbstractFetchDownloadService.ACTION_CANCEL)
            }

            is DownloadUIAction.RetryDownload -> {
                broadcastSender.sendBroadcast(action.downloadId, AbstractFetchDownloadService.ACTION_TRY_AGAIN)
            }

            else -> {}
        }
    }
}
