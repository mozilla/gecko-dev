/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import mozilla.components.browser.state.state.content.DownloadState

class FakeFileItemDescriptionProvider : FileItemDescriptionProvider {
    override fun getDescription(
        downloadState: DownloadState,
    ): String = when (downloadState.status) {
        DownloadState.Status.CANCELLED -> "Cancelled"
        DownloadState.Status.COMPLETED -> "Completed"
        DownloadState.Status.DOWNLOADING -> "Downloading"
        DownloadState.Status.FAILED -> "Failed"
        DownloadState.Status.INITIATED -> "Initiated"
        DownloadState.Status.PAUSED -> "Paused"
    }
}
