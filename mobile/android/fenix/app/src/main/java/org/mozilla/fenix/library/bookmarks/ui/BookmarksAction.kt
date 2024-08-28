/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.components.lib.state.Action

/**
 * Actions relating to the Bookmarks list screen and its various subscreens.
 */
internal sealed interface BookmarksAction : Action

/**
 * The Store is initializing.
 */
internal data object Init : BookmarksAction

/**
 * Bookmarks have been loaded from the storage layer.
 *
 * @property folderTitle The title of the bookmark folder that contains the loaded items.
 * @property bookmarkItems The bookmark items loaded, transformed into a displayable type.
 */
internal data class BookmarksLoaded(
    val folderTitle: String,
    val bookmarkItems: List<BookmarkItem>,
) : BookmarksAction

internal data class FolderClicked(val item: BookmarkItem.Folder) : BookmarksAction
internal data class BookmarkClicked(val item: BookmarkItem.Bookmark) : BookmarksAction
internal data object SearchClicked : BookmarksAction
