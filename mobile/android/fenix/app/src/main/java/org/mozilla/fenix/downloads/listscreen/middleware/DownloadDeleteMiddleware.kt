/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState

/**
 * Middleware for deleting a Download from disk.
 *
 * @param undoDelayProvider The [UndoDelayProvider] used to provide the undo delay.
 * @param removeDownloadUseCase The [DownloadsUseCases.RemoveDownloadUseCase] used to remove the download.
 * @param dispatcher The injected dispatcher used to run suspending operations on.
 */
class DownloadDeleteMiddleware(
    private val undoDelayProvider: UndoDelayProvider,
    private val removeDownloadUseCase: DownloadsUseCases.RemoveDownloadUseCase,
    private val dispatcher: CoroutineDispatcher = Dispatchers.Main,
) : Middleware<DownloadUIState, DownloadUIAction> {

    private var deleteJob: Job? = null

    /*
     * CoroutineScope used to launch the delete operation. This is a custom CoroutineScope with
     * an injected dispatcher, because the delete operations is short and should not be cancelled
     * when the UI is destroyed.
     */
    private val coroutineScope = CoroutineScope(dispatcher)

    override fun invoke(
        context: MiddlewareContext<DownloadUIState, DownloadUIAction>,
        next: (DownloadUIAction) -> Unit,
        action: DownloadUIAction,
    ) {
        next(action)
        when (action) {
            is DownloadUIAction.AddPendingDeletionSet ->
                startDelayedRemoval(context, action.itemIds, undoDelayProvider.undoDelay)

            is DownloadUIAction.UndoPendingDeletionSet -> deleteJob?.cancel()
            else -> {
                // no - op
            }
        }
    }

    private fun startDelayedRemoval(
        context: MiddlewareContext<DownloadUIState, DownloadUIAction>,
        items: Set<String>,
        delay: Long,
    ) {
        deleteJob = coroutineScope.launch {
            delay(delay)
            items.forEach { removeDownloadUseCase(it) }
            context.dispatch(DownloadUIAction.FileItemDeletedSuccessfully)
        }
    }
}
