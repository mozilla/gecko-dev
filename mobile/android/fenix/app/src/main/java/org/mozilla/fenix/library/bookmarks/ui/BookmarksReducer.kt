/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

/**
 * Function for reducing a new bookmarks state based on the received action.
 */
internal fun bookmarksReducer(state: BookmarksState, action: BookmarksAction) = when (action) {
    is BookmarksLoaded -> state.copy(
        folderTitle = action.folderTitle,
        bookmarkItems = action.bookmarkItems,
    )
    is FolderClicked,
    is BookmarkClicked,
    SearchClicked,
    Init,
    -> state
}
