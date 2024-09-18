/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
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
 * @param exitBookmarks Invoked when back is clicked while the navController's backstack is empty.
 * @param resolveFolderTitle Invoked to lookup user-friendly bookmark titles.
 * @param getBrowsingMode Invoked when retrieving the app's current [BrowsingMode].
 * @param openTab Invoked when opening a tab when a bookmark is clicked.
 * @param ioDispatcher Coroutine dispatcher for IO operations.
 */
internal class BookmarksMiddleware(
    private val bookmarksStorage: BookmarksStorage,
    private val navController: NavController,
    private val exitBookmarks: () -> Unit,
    private val resolveFolderTitle: (BookmarkNode) -> String,
    private val getBrowsingMode: () -> BrowsingMode,
    private val openTab: (url: String, openInNewTab: Boolean) -> Unit,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : Middleware<BookmarksState, BookmarksAction> {

    private val scope = CoroutineScope(Dispatchers.Main)

    @Suppress("LongMethod", "CyclomaticComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BookmarksState, BookmarksAction>,
        next: (BookmarksAction) -> Unit,
        action: BookmarksAction,
    ) {
        val preReductionState = context.state
        next(action)
        when (action) {
            Init -> scope.launch {
                loadTree(BookmarkRoot.Mobile.id)?.let { (folderTitle, bookmarkItems) ->
                    context.store.dispatch(
                        BookmarksLoaded(
                            folderTitle = folderTitle,
                            folderGuid = BookmarkRoot.Mobile.id,
                            bookmarkItems = bookmarkItems,
                        ),
                    )
                }
            }
            is BookmarkClicked -> {
                val openInNewTab = navController.previousDestinationWasHome() ||
                    getBrowsingMode() == BrowsingMode.Private
                openTab(action.item.url, openInNewTab)
            }
            is FolderClicked -> scope.launch {
                loadTree(action.item.guid)?.let { (folderTitle, bookmarkItems) ->
                    context.store.dispatch(
                        BookmarksLoaded(
                            folderTitle = folderTitle,
                            folderGuid = action.item.guid,
                            bookmarkItems = bookmarkItems,
                        ),
                    )
                }
            }
            SearchClicked -> navController.navigate(
                NavGraphDirections.actionGlobalSearchDialog(sessionId = null),
            )
            AddFolderClicked -> navController.navigate(BookmarksDestinations.ADD_FOLDER)
            BackClicked -> {
                when {
                    // non-list screen cases need to come first, since we presume if all subscreen
                    // state is null then we are on the list screen
                    preReductionState.bookmarksAddFolderState != null -> {
                        navController.popBackStack()
                        scope.launch(ioDispatcher) {
                            val newFolderTitle = preReductionState.bookmarksAddFolderState.folderBeingAddedTitle
                            if (newFolderTitle.isNotEmpty()) {
                                bookmarksStorage.addFolder(
                                    parentGuid = preReductionState.folderGuid,
                                    title = newFolderTitle,
                                )
                            }
                            loadTree(preReductionState.folderGuid)?.let { (folderTitle, bookmarkItems) ->
                                context.store.dispatch(
                                    BookmarksLoaded(
                                        folderTitle = folderTitle,
                                        folderGuid = preReductionState.folderGuid,
                                        bookmarkItems = bookmarkItems,
                                    ),
                                )
                            }
                        }
                    }
                    // list screen cases
                    preReductionState.folderGuid != BookmarkRoot.Mobile.id -> {
                        scope.launch {
                            val parentFolderGuid = withContext(ioDispatcher) {
                                bookmarksStorage
                                    .getBookmark(preReductionState.folderGuid)
                                    ?.parentGuid ?: BookmarkRoot.Mobile.id
                            }
                            loadTree(parentFolderGuid)?.let { (folderTitle, bookmarkItems) ->
                                context.store.dispatch(
                                    BookmarksLoaded(
                                        folderTitle = folderTitle,
                                        folderGuid = parentFolderGuid,
                                        bookmarkItems = bookmarkItems,
                                    ),
                                )
                            }
                        }
                    }
                    else -> {
                        if (!navController.popBackStack()) {
                            exitBookmarks()
                        }
                    }
                }
            }
            AddFolderAction.ParentFolderClicked -> {
                // TODO this will navigate to the select folder screen
            }
            is BookmarkLongClicked,
            is FolderLongClicked,
            is BookmarksLoaded,
            is AddFolderAction.TitleChanged,
            -> Unit
        }
    }

    private suspend fun loadTree(guid: String): Pair<String, List<BookmarkItem>>? =
        withContext(ioDispatcher) {
            bookmarksStorage.getTree(guid)?.let { rootNode ->
                resolveFolderTitle(rootNode) to rootNode.childItems()
            }
        }

    private fun BookmarkNode.childItems(): List<BookmarkItem> = this.children?.mapNotNull { node ->
        Result.runCatching {
            when (node.type) {
                BookmarkNodeType.ITEM -> BookmarkItem.Bookmark(
                    url = node.url!!,
                    title = node.title!!,
                    previewImageUrl = node.url!!,
                    guid = node.guid,
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
