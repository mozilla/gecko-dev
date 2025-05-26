/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

internal fun BaseBrowserFragment.handleOnDownloadFinished(
    appStore: AppStore,
    downloadState: DownloadState,
    downloadJobStatus: Status,
    browserToolbars: List<ScrollableToolbar>,
) {
    // If the download is just paused, don't show any in-app notification
    if (shouldShowCompletedDownloadDialog(downloadState, downloadJobStatus)) {
        val safeContext = context ?: return

        if (downloadState.openInApp && downloadJobStatus == Status.COMPLETED) {
            val fileWasOpened = AbstractFetchDownloadService.openFile(
                applicationContext = safeContext.applicationContext,
                downloadFileName = downloadState.fileName,
                downloadFilePath = downloadState.filePath,
                downloadContentType = downloadState.contentType,
            )
            if (!fileWasOpened) {
                appStore.dispatch(
                    AppAction.DownloadAction.CannotOpenFile(downloadState = downloadState),
                )
            }
        } else {
            if (downloadJobStatus == Status.FAILED) {
                appStore.dispatch(
                    AppAction.DownloadAction.DownloadFailed(
                        downloadState.fileName,
                    ),
                )
            } else {
                appStore.dispatch(
                    AppAction.DownloadAction.DownloadCompleted(
                        downloadState,
                    ),
                )
            }

            browserToolbars.forEach { it.expand() }
        }
    }
}
