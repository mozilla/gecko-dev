/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

/**
 * A middleware for handling side-effects in response to [BookmarksAction]s.
 *
 * @param bookmarksStorage Storage layer for reading and writing bookmarks.
 * @param navController NavController for navigating to a tab, a search fragment, etc.
 * @param exitBookmarks Invoked when back is clicked while the navController's backstack is empty.
 * @param navigateToSignIntoSync Invoked when handling [SignIntoSyncClicked].
 * @param resolveFolderTitle Invoked to lookup user-friendly bookmark titles.
 * @param getBrowsingMode Invoked when retrieving the app's current [BrowsingMode].
 * @param openTab Invoked when opening a tab when a bookmark is clicked.
 * @param ioDispatcher Coroutine dispatcher for IO operations.
 */
@Suppress("LongParameterList")
internal class BookmarksMiddleware(
    private val bookmarksStorage: BookmarksStorage,
    private val navController: NavController,
    private val exitBookmarks: () -> Unit,
    private val navigateToSignIntoSync: () -> Unit,
    private val resolveFolderTitle: (BookmarkNode) -> String,
    private val getBrowsingMode: () -> BrowsingMode,
    private val openTab: (url: String, openInNewTab: Boolean) -> Unit,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : Middleware<BookmarksState, BookmarksAction> {

    private val scope = CoroutineScope(ioDispatcher)

    @Suppress("LongMethod", "ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BookmarksState, BookmarksAction>,
        next: (BookmarksAction) -> Unit,
        action: BookmarksAction,
    ) {
        val preReductionState = context.state
        next(action)
        when (action) {
            Init -> context.store.tryDispatchLoadFor(BookmarkRoot.Mobile.id)
            is BookmarkClicked -> {
                val openInNewTab = navController.previousDestinationWasHome() ||
                    getBrowsingMode() == BrowsingMode.Private
                openTab(action.item.url, openInNewTab)
            }
            is FolderClicked -> context.store.tryDispatchLoadFor(action.item.guid)
            SearchClicked -> navController.navigate(
                NavGraphDirections.actionGlobalSearchDialog(sessionId = null),
            )
            AddFolderClicked -> navController.navigate(BookmarksDestinations.ADD_FOLDER)
            SignIntoSyncClicked -> navigateToSignIntoSync()
            is EditBookmarkClicked -> navController.navigate(BookmarksDestinations.EDIT_BOOKMARK)
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
                                    parentGuid = preReductionState.currentFolder.guid,
                                    title = newFolderTitle,
                                )
                            }
                            context.store.tryDispatchLoadFor(preReductionState.currentFolder.guid)
                        }
                    }
                    preReductionState.bookmarksEditBookmarkState != null -> {
                        navController.popBackStack()
                        scope.launch(ioDispatcher) {
                            preReductionState.createBookmarkInfo()?.also {
                                bookmarksStorage.updateNode(
                                    guid = preReductionState.bookmarksEditBookmarkState.bookmark.guid,
                                    info = it,
                                )
                            }
                            context.store.tryDispatchLoadFor(preReductionState.currentFolder.guid)
                        }
                    }
                    // list screen cases
                    preReductionState.currentFolder.guid != BookmarkRoot.Mobile.id -> {
                        scope.launch {
                            val parentFolderGuid = withContext(ioDispatcher) {
                                bookmarksStorage
                                    .getBookmark(preReductionState.currentFolder.guid)
                                    ?.parentGuid ?: BookmarkRoot.Mobile.id
                            }
                            context.store.tryDispatchLoadFor(parentFolderGuid)
                        }
                    }
                    else -> {
                        if (!navController.popBackStack()) {
                            exitBookmarks()
                        }
                    }
                }
            }
            EditBookmarkAction.FolderClicked -> { /* Navigate to folder picker */ }
            EditBookmarkAction.DeleteClicked -> {
                // Bug 1919949 — Add undo snackbar to delete action.
                navController.popBackStack()
                scope.launch(ioDispatcher) {
                    preReductionState.bookmarksEditBookmarkState?.also {
                        bookmarksStorage.deleteNode(it.bookmark.guid)
                    }
                    context.store.tryDispatchLoadFor(preReductionState.currentFolder.guid)
                }
            }
            AddFolderAction.ParentFolderClicked -> {
                // TODO this will navigate to the select folder screen
            }
            SelectFolderAction.ViewAppeared -> context.store.loadFolders(BookmarkRoot.Mobile.id)
            is EditBookmarkAction.TitleChanged,
            is EditBookmarkAction.URLChanged,
            is BookmarkLongClicked,
            is FolderLongClicked,
            is BookmarksLoaded,
            is AddFolderAction.TitleChanged,
            is SelectFolderAction.FoldersLoaded,
            -> Unit
        }
    }

    private fun Store<BookmarksState, BookmarksAction>.loadFolders(guid: String) =
        scope.launch {
            bookmarksStorage.getTree(guid, recursive = true)?.let { rootNode ->
                val folders = collectFolders(rootNode)
                dispatch(SelectFolderAction.FoldersLoaded(folders))
            }
        }

    private fun Store<BookmarksState, BookmarksAction>.tryDispatchLoadFor(guid: String) =
        scope.launch {
            bookmarksStorage.getTree(guid)?.let { rootNode ->
                val folderTitle = resolveFolderTitle(rootNode)
                val items = rootNode.childItems()
                dispatch(
                    BookmarksLoaded(
                        folder = BookmarkItem.Folder(
                            guid = guid,
                            title = folderTitle,
                        ),
                        bookmarkItems = items,
                    ),
                )
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

    private fun collectFolders(
        node: BookmarkNode,
        indentation: Int = 0,
        folders: MutableList<SelectFolderItem> = mutableListOf(),
    ): List<SelectFolderItem> {
        if (node.type == BookmarkNodeType.FOLDER) {
            folders.add(
                SelectFolderItem(
                    indentation = indentation,
                    folder = BookmarkItem.Folder(
                        guid = node.guid,
                        title = resolveFolderTitle(node),
                    ),
                ),
            )

            node.children?.forEach { child ->
                folders.addAll(collectFolders(child, indentation + 1))
            }
        }

        return folders
    }
}

@VisibleForTesting(otherwise = PRIVATE)
internal fun BookmarksState.createBookmarkInfo() = bookmarksEditBookmarkState?.let { state ->
    BookmarkInfo(
        parentGuid = state.folder.guid,
        position = bookmarkItems.indexOfFirst { it.guid == state.bookmark.guid }.toUInt(),
        title = state.bookmark.title,
        url = state.bookmark.url,
    )
}
