/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Downloads
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState

/**
 * A [Middleware] for recording telemetry based on [DownloadUIState]s that are dispatch to the
 * [DownloadUIStore].
 */
class DownloadTelemetryMiddleware : Middleware<DownloadUIState, DownloadUIAction> {

    override fun invoke(
        context: MiddlewareContext<DownloadUIState, DownloadUIAction>,
        next: (DownloadUIAction) -> Unit,
        action: DownloadUIAction,
    ) {
        next(action)
        when (action) {
            is DownloadUIAction.Init -> {
                Downloads.screenViewed.record(NoExtras())
            }

            is DownloadUIAction.FileItemDeletedSuccessfully -> {
                Downloads.deleted.record(NoExtras())
            }

            is DownloadUIAction.ContentTypeSelected -> {
                Downloads.filtered.set(action.contentTypeFilter.name)
            }

            is DownloadUIAction.ShareUrlClicked -> {
                Downloads.shareUrl.record(NoExtras())
            }

            is DownloadUIAction.ShareFileClicked -> {
                Downloads.shareFile.record(NoExtras())
            }

            is DownloadUIAction.PauseDownload -> {
                Downloads.pauseDownload.record(NoExtras())
            }

            is DownloadUIAction.ResumeDownload -> {
               Downloads.resumeDownload.record(NoExtras())
            }

            is DownloadUIAction.CancelDownload -> {
                Downloads.cancelDownload.record(NoExtras())
            }

            is DownloadUIAction.RetryDownload -> {
                Downloads.retryDownload.record(NoExtras())
            }

            else -> {}
        }
    }
}
