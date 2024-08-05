/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * A middleware for handling side-effects in response to [BookmarksAction]s.
 *
 * @param bookmarksStorage Storage layer for reading and writing bookmarks.
 * @param scope Coroutine scope for async operations.
 */
internal class BookmarksMiddleware(
    private val bookmarksStorage: BookmarksStorage,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<BookmarksState, BookmarksAction> {
    override fun invoke(
        context: MiddlewareContext<BookmarksState, BookmarksAction>,
        next: (BookmarksAction) -> Unit,
        action: BookmarksAction,
    ) {
        next(action)
        when (action) {
            Init -> scope.launch {
                val bookmarkItems = bookmarksStorage
                    .getTree(BookmarkRoot.Mobile.id)
                    ?.toBookmarkItems()
                    ?: listOf()
                context.store.dispatch(BookmarksLoaded(bookmarkItems))
            }

            is BookmarksLoaded -> Unit
        }
    }

    private fun BookmarkNode.toBookmarkItems(): List<BookmarkItem> = this.children?.mapNotNull { node ->
        Result.runCatching {
            when (node.type) {
                BookmarkNodeType.ITEM -> BookmarkItem.Bookmark(
                    url = node.url!!,
                    title = node.title!!,
                    previewImageUrl = node.url!!,
                )
                BookmarkNodeType.FOLDER -> BookmarkItem.Folder(
                    name = node.title!!,
                )
                BookmarkNodeType.SEPARATOR -> null
            }
        }.getOrNull()
    } ?: listOf()
}
