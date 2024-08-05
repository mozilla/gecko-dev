/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

/**
 * Items that can be represented in the Bookmarks list.
 */
internal sealed class BookmarkItem {

    /**
     * An item representing a site that is bookmarked.
     *
     * @property url The bookmarked url.
     * @property title The title of the bookmark.
     * @property previewImageUrl The url to lookup the favicon for the bookmark.
     */
    data class Bookmark(val url: String, val title: String, val previewImageUrl: String) : BookmarkItem()

    /**
     * An item representing a bookmark folder.
     *
     * @property name The name of the folder.
     */
    data class Folder(val name: String) : BookmarkItem()
}
