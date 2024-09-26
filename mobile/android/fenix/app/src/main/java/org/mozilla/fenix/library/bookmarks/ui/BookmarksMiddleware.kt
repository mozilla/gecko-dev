/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import android.content.ClipData
import android.content.ClipboardManager
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
import mozilla.components.feature.tabs.TabsUseCases
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
 * @param clipboardManager For copying bookmark URLs.
 * @param addNewTabUseCase For opening tabs from menus.
 * @param navController NavController for navigating to a tab, a search fragment, etc.
 * @param exitBookmarks Invoked when back is clicked while the navController's backstack is empty.
 * @param navigateToSignIntoSync Invoked when handling [SignIntoSyncClicked].
 * @param shareBookmark Invoked when the share option is selected from a menu.
 * @param showTabsTray Invoked after opening tabs from menus.
 * @param resolveFolderTitle Invoked to lookup user-friendly bookmark titles.
 * @param showUrlCopiedSnackbar Invoked when a bookmark url is copied.
 * @param getBrowsingMode Invoked when retrieving the app's current [BrowsingMode].
 * @param openTab Invoked when opening a tab when a bookmark is clicked.
 * @param ioDispatcher Coroutine dispatcher for IO operations.
 */
@Suppress("LongParameterList")
internal class BookmarksMiddleware(
    private val bookmarksStorage: BookmarksStorage,
    private val clipboardManager: ClipboardManager?,
    private val addNewTabUseCase: TabsUseCases.AddNewTabUseCase,
    private val navController: NavController,
    private val exitBookmarks: () -> Unit,
    private val navigateToSignIntoSync: () -> Unit,
    private val shareBookmark: (url: String, title: String) -> Unit,
    private val showTabsTray: (isPrivateMode: Boolean) -> Unit,
    private val resolveFolderTitle: (BookmarkNode) -> String,
    private val showUrlCopiedSnackbar: () -> Unit,
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
                if (preReductionState.selectedItems.isNotEmpty()) {
                    return
                }
                val openInNewTab = navController.previousDestinationWasHome() ||
                    getBrowsingMode() == BrowsingMode.Private
                openTab(action.item.url, openInNewTab)
            }

            is FolderClicked -> {
                if (preReductionState.selectedItems.isNotEmpty()) {
                    return
                }
                context.store.tryDispatchLoadFor(action.item.guid)
            }

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
                    preReductionState.bookmarksSelectFolderState != null -> {
                        navController.popBackStack()
                    }

                    preReductionState.bookmarksEditFolderState != null -> {
                        val editState = preReductionState.bookmarksEditFolderState
                        navController.popBackStack()
                        scope.launch(ioDispatcher) {
                            if (editState.folder.title.isNotEmpty()) {
                                preReductionState.createBookmarkInfo()?.also {
                                    bookmarksStorage.updateNode(editState.folder.guid, it)
                                }
                            }
                            context.store.tryDispatchLoadFor(preReductionState.currentFolder.guid)
                        }
                    }

                    preReductionState.bookmarksAddFolderState != null -> {
                        navController.popBackStack()
                        scope.launch(ioDispatcher) {
                            val newFolderTitle =
                                preReductionState.bookmarksAddFolderState.folderBeingAddedTitle
                            if (newFolderTitle.isNotEmpty()) {
                                bookmarksStorage.addFolder(
                                    parentGuid = preReductionState.bookmarksAddFolderState.parent.guid,
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

            EditBookmarkAction.FolderClicked -> {
                navController.navigate(BookmarksDestinations.SELECT_FOLDER)
            }

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
            EditFolderAction.ParentFolderClicked,
            AddFolderAction.ParentFolderClicked,
            -> {
                navController.navigate(BookmarksDestinations.SELECT_FOLDER)
            }

            SelectFolderAction.ViewAppeared -> context.store.tryDispatchLoadFolders()
            is BookmarksListMenuAction -> action.handleSideEffects(context.store, preReductionState)
            is EditBookmarkAction.TitleChanged,
            is EditBookmarkAction.URLChanged,
            is BookmarkLongClicked,
            is FolderLongClicked,
            is BookmarksLoaded,
            is EditFolderAction.TitleChanged,
            is AddFolderAction.TitleChanged,
            is SelectFolderAction.FoldersLoaded,
            is SelectFolderAction.ItemClicked,
            -> Unit
        }
    }

    private fun Store<BookmarksState, BookmarksAction>.tryDispatchLoadFolders() =
        scope.launch {
            val includeDesktop = state.isSignedIntoSync || bookmarksStorage.hasDesktopBookmarks()

            val folders = if (includeDesktop) {
                bookmarksStorage.getTree(BookmarkRoot.Root.id, recursive = true)?.let { rootNode ->
                    val excludingMobile =
                        rootNode.children?.filterNot { it.guid == BookmarkRoot.Mobile.id }
                    val desktopRoot = rootNode.copy(children = excludingMobile)
                    rootNode.children?.find { it.guid == BookmarkRoot.Mobile.id }?.let {
                        val newChildren = listOf(desktopRoot) + it.children.orEmpty()
                        it.copy(children = newChildren)
                    }?.let { collectFolders(it) }
                }
            } else {
                bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = true)
                    ?.let { rootNode ->
                        collectFolders(rootNode)
                    }
            }

            folders?.also { dispatch(SelectFolderAction.FoldersLoaded(it)) }
        }

    private fun Store<BookmarksState, BookmarksAction>.tryDispatchLoadFor(guid: String) =
        scope.launch {
            bookmarksStorage.getTree(guid)?.let { rootNode ->
                val folder = BookmarkItem.Folder(
                    guid = guid,
                    title = resolveFolderTitle(rootNode),
                )

                val items = when (guid) {
                    BookmarkRoot.Root.id -> {
                        rootNode.copy(
                            children = rootNode.children
                                ?.filterNot { it.guid == BookmarkRoot.Mobile.id }
                                ?.map { it.copy(title = resolveFolderTitle(it)) },
                        ).childItems()
                    }
                    BookmarkRoot.Mobile.id -> {
                        if (state.isSignedIntoSync || bookmarksStorage.hasDesktopBookmarks()) {
                            val desktopNode = bookmarksStorage.getTree(BookmarkRoot.Root.id)?.let {
                                it.copy(title = resolveFolderTitle(it))
                            }

                            val mobileRoot = rootNode.copy(
                                children = listOfNotNull(desktopNode) + rootNode.children.orEmpty(),
                            )
                            mobileRoot.childItems()
                        } else {
                            rootNode.childItems()
                        }
                    }
                    else -> rootNode.childItems()
                }

                dispatch(
                    BookmarksLoaded(
                        folder = folder,
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

    @Suppress("LongMethod")
    private fun BookmarksListMenuAction.handleSideEffects(
        store: Store<BookmarksState, BookmarksAction>,
        preReductionState: BookmarksState,
    ) {
        when (this) {
            // bookmark menu actions
            is BookmarksListMenuAction.Bookmark.EditClicked -> {
                navController.navigate(BookmarksDestinations.EDIT_BOOKMARK)
            }

            is BookmarksListMenuAction.Bookmark.CopyClicked -> {
                val urlClipData = ClipData.newPlainText(bookmark.url, bookmark.url)
                clipboardManager?.setPrimaryClip(urlClipData)
                showUrlCopiedSnackbar()
            }

            is BookmarksListMenuAction.Bookmark.ShareClicked -> {
                shareBookmark(bookmark.url, bookmark.title)
            }

            is BookmarksListMenuAction.Bookmark.OpenInNormalTabClicked -> {
                // Bug 1919949 — Add undo snackbar to delete action.
                addNewTabUseCase(url = bookmark.url, private = false)
                showTabsTray(false)
            }

            is BookmarksListMenuAction.Bookmark.OpenInPrivateTabClicked -> {
                addNewTabUseCase(url = bookmark.url, private = true)
                showTabsTray(true)
            }

            is BookmarksListMenuAction.Bookmark.DeleteClicked -> scope.launch {
                // Bug 1919949 — Add undo snackbar to delete action.
                bookmarksStorage.deleteNode(bookmark.guid)
                store.tryDispatchLoadFor(store.state.currentFolder.guid)
            }

            // folder menu actions
            is BookmarksListMenuAction.Folder.EditClicked -> {
                navController.navigate(BookmarksDestinations.EDIT_FOLDER)
            }

            is BookmarksListMenuAction.Folder.OpenAllInNormalTabClicked -> scope.launch {
                bookmarksStorage.getTree(folder.guid)?.children
                    ?.mapNotNull { it.url }
                    ?.forEach { url -> addNewTabUseCase(url = url, private = false) }
                withContext(Dispatchers.Main) {
                    showTabsTray(false)
                }
            }

            is BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked -> scope.launch {
                bookmarksStorage.getTree(folder.guid)?.children
                    ?.mapNotNull { it.url }
                    ?.forEach { url -> addNewTabUseCase(url = url, private = true) }
                withContext(Dispatchers.Main) {
                    showTabsTray(true)
                }
            }

            is BookmarksListMenuAction.Folder.DeleteClicked -> scope.launch {
                // Bug 1919949 — Add undo snackbar to delete action.
                bookmarksStorage.deleteNode(folder.guid)
                store.tryDispatchLoadFor(store.state.currentFolder.guid)
            }

            // top bar menu actions
            BookmarksListMenuAction.MultiSelect.EditClicked -> {
                navController.navigate(BookmarksDestinations.EDIT_BOOKMARK)
            }

            BookmarksListMenuAction.MultiSelect.MoveClicked -> {
                // TODO
            }

            BookmarksListMenuAction.MultiSelect.OpenInNormalTabsClicked -> scope.launch {
                preReductionState.selectedItems
                    .mapNotNull { (it as? BookmarkItem.Bookmark)?.url }
                    .forEach { url -> addNewTabUseCase(url = url, private = false) }
                withContext(Dispatchers.Main) {
                    showTabsTray(false)
                }
            }

            BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked -> scope.launch {
                preReductionState.selectedItems
                    .mapNotNull { (it as? BookmarkItem.Bookmark)?.url }
                    .forEach { url -> addNewTabUseCase(url = url, private = true) }
                withContext(Dispatchers.Main) {
                    showTabsTray(true)
                }
            }

            BookmarksListMenuAction.MultiSelect.ShareClicked -> {
                preReductionState.selectedItems.filterIsInstance<BookmarkItem.Bookmark>()
                    .forEach { shareBookmark(it.url, it.title) }
            }

            BookmarksListMenuAction.MultiSelect.DeleteClicked -> scope.launch {
                // TODO confirm deletion
                preReductionState.selectedItems.forEach { item ->
                    bookmarksStorage.deleteNode(item.guid)
                }
                store.tryDispatchLoadFor(preReductionState.currentFolder.guid)
            }
        }
    }
}

private suspend fun BookmarksStorage.hasDesktopBookmarks(): Boolean {
    return countBookmarksInTrees(
        listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id),
    ) > 0u
}

private fun BookmarksState.createBookmarkInfo() = when {
    bookmarksEditFolderState != null -> bookmarksEditFolderState.let { state ->
        BookmarkInfo(
            parentGuid = state.parent.guid,
            position = bookmarkItems.indexOfFirst { it.guid == state.folder.guid }.toUInt(),
            title = state.folder.title,
            url = null,
        )
    }
    bookmarksEditBookmarkState != null -> bookmarksEditBookmarkState.let { state ->
        BookmarkInfo(
            parentGuid = state.folder.guid,
            position = bookmarkItems.indexOfFirst { it.guid == state.bookmark.guid }.toUInt(),
            title = state.bookmark.title,
            url = state.bookmark.url,
        )
    }
    else -> null
}
