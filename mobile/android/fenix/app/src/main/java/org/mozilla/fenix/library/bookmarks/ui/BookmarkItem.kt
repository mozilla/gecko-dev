/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.components.concept.storage.BookmarkNode

/**
 * Items that can be represented in the Bookmarks list.
 */
internal sealed class BookmarkItem {

    abstract val guid: String
    abstract val title: String
    abstract val dateAdded: Long

    /**
     * An item representing a site that is bookmarked.
     *
     * @property url The bookmarked url.
     * @property title The title of the bookmark.
     * @property previewImageUrl The url to lookup the favicon for the bookmark.
     * @property guid The guid of the [BookmarkNode] representing this bookmark.
     * @property dateAdded Timestamp in milliseconds when the Bookmark Item was added.
     */
    data class Bookmark(
        val url: String,
        override val title: String,
        val previewImageUrl: String,
        override val guid: String,
        override val dateAdded: Long = 0,
    ) : BookmarkItem()

    /**
     * An item representing a bookmark folder.
     *
     * @property title The name of the folder.
     * @property guid The guid of the [BookmarkNode] representing this folder.
     * @property dateAdded Timestamp in milliseconds when the Bookmark Item was added.
     */
    data class Folder(
        override val title: String,
        override val guid: String,
        override val dateAdded: Long = 0,
    ) : BookmarkItem()
}
