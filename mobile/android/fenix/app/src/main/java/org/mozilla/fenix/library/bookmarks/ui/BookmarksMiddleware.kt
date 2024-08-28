/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

/**
 * A middleware for handling side-effects in response to [BookmarksAction]s.
 *
 * @param bookmarksStorage Storage layer for reading and writing bookmarks.
 * @param navController NavController for navigating to a tab, a search fragment, etc.
 * @param resolveFolderTitle Invoked to lookup user-friendly bookmark titles.
 * @param getBrowsingMode Invoked when retrieving the app's current [BrowsingMode].
 * @param openTab Invoked when opening a tab when a bookmark is clicked.
 * @param scope Coroutine scope for async operations.
 */
internal class BookmarksMiddleware(
    private val bookmarksStorage: BookmarksStorage,
    private val navController: NavController,
    private val resolveFolderTitle: (BookmarkNode) -> String,
    private val getBrowsingMode: () -> BrowsingMode,
    private val openTab: (url: String, openInNewTab: Boolean) -> Unit,
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
                loadTree(BookmarkRoot.Mobile.id)?.let { (folderTitle, bookmarkItems) ->
                    context.store.dispatch(BookmarksLoaded(folderTitle, bookmarkItems))
                }
            }
            is BookmarkClicked -> scope.launch(Dispatchers.Main) {
                val openInNewTab = navController.previousDestinationWasHome() ||
                    getBrowsingMode() == BrowsingMode.Private
                openTab(action.item.url, openInNewTab)
            }
            is FolderClicked -> scope.launch {
                loadTree(action.item.guid)?.let { (folderTitle, bookmarkItems) ->
                    context.store.dispatch(BookmarksLoaded(folderTitle, bookmarkItems))
                }
            }
            SearchClicked -> scope.launch(Dispatchers.Main) {
                navController.navigate(
                    NavGraphDirections.actionGlobalSearchDialog(sessionId = null),
                )
            }
            is BookmarksLoaded -> Unit
        }
    }

    private suspend fun loadTree(guid: String): Pair<String, List<BookmarkItem>>? =
        bookmarksStorage.getTree(guid)?.let { rootNode ->
            resolveFolderTitle(rootNode) to rootNode.childItems()
        }

    private fun BookmarkNode.childItems(): List<BookmarkItem> = this.children?.mapNotNull { node ->
        Result.runCatching {
            when (node.type) {
                BookmarkNodeType.ITEM -> BookmarkItem.Bookmark(
                    url = node.url!!,
                    title = node.title!!,
                    previewImageUrl = node.url!!,
                )
                BookmarkNodeType.FOLDER -> BookmarkItem.Folder(
                    title = node.title!!,
                    guid = node.guid,
                )
                BookmarkNodeType.SEPARATOR -> null
            }
        }.getOrNull()
    } ?: listOf()

    private fun NavController.previousDestinationWasHome(): Boolean =
        previousBackStackEntry?.destination?.id == R.id.homeFragment
}
