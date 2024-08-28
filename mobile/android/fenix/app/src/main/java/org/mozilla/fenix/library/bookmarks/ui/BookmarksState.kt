/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.components.lib.state.State

/**
 * Represents the state of the Bookmarks list screen and its various subscreens.
 *
 * @property bookmarkItems Bookmark items to be displayed in the current list screen.
 * @property folderTitle The title of currently selected folder whose children items are being displayed.
 */
internal data class BookmarksState(
    val bookmarkItems: List<BookmarkItem>,
    val folderTitle: String,
) : State {
    companion object {
        val default: BookmarksState = BookmarksState(
            bookmarkItems = listOf(),
            "",
        )
    }
}
