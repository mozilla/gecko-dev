/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.annotation.VisibleForTesting
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.components.AppStore

/**
 * A middleware that supports private browsing mode lock feature on bookmarks screen.
 *
 * If private mode is locked and requires verification to access it, the middleware intercepts
 * private mode related actions and requires verification before allowing them to proceed.
 *
 * @param appStore The [AppStore] containing the state of private mode lock feature.
 * @param requireAuth A callback function that triggers the UI flow to authenticate the user.
 */
internal class PrivateBrowsingLockMiddleware(
    private val appStore: AppStore,
    private val requireAuth: () -> Unit,
) : Middleware<BookmarksState, BookmarksAction> {

    @VisibleForTesting
    internal var pendingAction: BookmarksAction? = null

    override fun invoke(
        context: MiddlewareContext<BookmarksState, BookmarksAction>,
        next: (BookmarksAction) -> Unit,
        action: BookmarksAction,
    ) {
        when (action) {
            is BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked,
            is BookmarksListMenuAction.Bookmark.OpenInPrivateTabClicked,
            is BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked,
                -> {
                if (appStore.state.isPrivateScreenLocked) {
                    pendingAction = action
                    requireAuth()
                    // We intentionally intercept this action to prevent other middleware
                    // from processing it, until the user is verified.
                    return
                }
            }

            is PrivateBrowsingAuthorized -> {
                pendingAction?.let { next(it) }
                pendingAction = null
            }

            else -> Unit
        }

        next(action)
    }
}
