/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.cancellable
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.onEach
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.ext.flow
import mozilla.components.service.fxa.store.SyncStatus
import mozilla.components.service.fxa.store.SyncStore

internal class BookmarksSyncMiddleware(
    private val syncStore: SyncStore,
    private val scope: CoroutineScope,
) : Middleware<BookmarksState, BookmarksAction> {
    override fun invoke(
        context: MiddlewareContext<BookmarksState, BookmarksAction>,
        next: (BookmarksAction) -> Unit,
        action: BookmarksAction,
    ) {
        next(action)
        when (action) {
            Init -> {
                // Observe for the account to become signed-in, and then wait for the first
                // instance of the Sync Engine to finish so we know it's safe to load bookmarks
                syncStore.flow()
                    .map { it.account != null }
                    .distinctUntilChanged()
                    .onEach { isSignedIn ->
                        context.store.dispatch(ReceivedSyncSignInUpdate(isSignedIn))
                        if (isSignedIn) {
                            syncStore.flow()
                                .map { it.status == SyncStatus.Idle }
                                .onEach { isIdle ->
                                    if (isIdle) {
                                        throw CancellationException()
                                    }
                                }
                                .cancellable()
                                .catch { context.store.dispatch(FirstSyncCompleted) }
                                .launchIn(scope)
                        }
                    }
                    .launchIn(scope)
            }

            else -> Unit
        }
    }
}
