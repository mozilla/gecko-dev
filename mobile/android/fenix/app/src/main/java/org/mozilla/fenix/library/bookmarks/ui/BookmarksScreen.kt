/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.library.bookmarks.ui

import androidx.activity.compose.BackHandler
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.paddingFromBaseline
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.AlertDialog
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ContextualMenu
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.TextField
import org.mozilla.fenix.compose.TextFieldColors
import org.mozilla.fenix.compose.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.compose.core.Action
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.compose.list.SelectableFaviconListItem
import org.mozilla.fenix.compose.list.SelectableIconListItem
import org.mozilla.fenix.compose.snackbar.AcornSnackbarHostState
import org.mozilla.fenix.compose.snackbar.SnackbarHost
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.theme.FirefoxTheme
import mozilla.components.ui.icons.R as iconsR

/**
 * The UI host for the Bookmarks list screen and related subscreens.
 *
 * @param buildStore A builder function to construct a [BookmarksStore] using the NavController that's local
 * to the nav graph for the Bookmarks view hierarchy.
 * @param startDestination the screen on which to initialize [BookmarksScreen] with.
 */
@Composable
internal fun BookmarksScreen(
    buildStore: (NavHostController) -> BookmarksStore,
    startDestination: String = BookmarksDestinations.LIST,
) {
    val navController = rememberNavController()
    val store = buildStore(navController)

    DisposableEffect(LocalLifecycleOwner.current) {
        onDispose {
            store.dispatch(ViewDisposed)
        }
    }
    NavHost(
        navController = navController,
        startDestination = startDestination,
    ) {
        composable(route = BookmarksDestinations.LIST) {
            BackHandler { store.dispatch(BackClicked) }
            BookmarksList(store = store)
        }
        composable(route = BookmarksDestinations.ADD_FOLDER) {
            BackHandler { store.dispatch(BackClicked) }
            AddFolderScreen(store = store)
        }
        composable(route = BookmarksDestinations.EDIT_FOLDER) {
            BackHandler { store.dispatch(BackClicked) }
            EditFolderScreen(store = store)
        }
        composable(route = BookmarksDestinations.EDIT_BOOKMARK) {
            BackHandler { store.dispatch(BackClicked) }
            EditBookmarkScreen(store = store)
        }
        composable(route = BookmarksDestinations.SELECT_FOLDER) {
            BackHandler { store.dispatch(BackClicked) }
            SelectFolderScreen(store = store)
        }
    }
}

internal object BookmarksDestinations {
    const val LIST = "list"
    const val ADD_FOLDER = "add folder"
    const val EDIT_FOLDER = "edit folder"
    const val EDIT_BOOKMARK = "edit bookmark"
    const val SELECT_FOLDER = "select folder"
}

/**
 * The Bookmarks list screen.
 */
@Suppress("LongMethod", "ComplexMethod")
@Composable
private fun BookmarksList(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state) { it }
    val scope = rememberCoroutineScope()
    val snackbarHostState = remember { AcornSnackbarHostState() }

    val snackbarMessage = when (state.bookmarksSnackbarState) {
        BookmarksSnackbarState.CantEditDesktopFolders -> stringResource(R.string.bookmark_cannot_edit_root)
        is BookmarksSnackbarState.UndoDeletion -> {
            val (id, titleOrCount) = state.undoSnackbarText()
            stringResource(id, titleOrCount)
        }
        else -> ""
    }

    val snackbarActionLabel = when (state.bookmarksSnackbarState) {
        is BookmarksSnackbarState.UndoDeletion -> stringResource(R.string.bookmark_undo_deletion)
        else -> null
    }
    val action: Action? = snackbarActionLabel?.let {
        Action(
            label = snackbarActionLabel,
            onClick = {
                store.dispatch(SnackbarAction.Undo)
            },
        )
    }

    LaunchedEffect(state.bookmarksSnackbarState) {
        when (state.bookmarksSnackbarState) {
            BookmarksSnackbarState.None -> return@LaunchedEffect
            is BookmarksSnackbarState.UndoDeletion -> scope.launch {
                snackbarHostState.showSnackbar(
                    snackbarState = SnackbarState(
                        message = snackbarMessage,
                        action = action,
                        onDismiss = {
                            store.dispatch(SnackbarAction.Dismissed)
                        },
                    ),
                )
            }
            BookmarksSnackbarState.CantEditDesktopFolders -> scope.launch {
                snackbarHostState.showSnackbar(
                    snackbarState = SnackbarState(
                        message = snackbarMessage,
                        onDismiss = {
                            store.dispatch(SnackbarAction.Dismissed)
                        },
                    ),
                )
            }
        }
    }

    WarnDialog(store = store)

    val dialogState = state.bookmarksDeletionDialogState
    if (dialogState is DeletionDialogState.Presenting) {
        AlertDialogDeletionWarning(
            onCancelTapped = { store.dispatch(DeletionDialogAction.CancelTapped) },
            onDeleteTapped = { store.dispatch(DeletionDialogAction.DeleteTapped) },
        )
    }

    Scaffold(
        snackbarHost = {
            Box(modifier = Modifier.fillMaxWidth()) {
                SnackbarHost(
                    modifier = Modifier.align(Alignment.BottomCenter),
                    snackbarHostState = snackbarHostState,
                )
            }
        },
        floatingActionButton = {
            FloatingActionButton(
                icon = painterResource(R.drawable.mozac_ic_search_24),
                contentDescription = stringResource(R.string.bookmark_search_button_content_description),
                onClick = { store.dispatch(SearchClicked) },
            )
        },
        topBar = {
            BookmarksListTopBar(store = store)
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        val emptyListState = state.emptyListState()
        if (emptyListState != null) {
            EmptyList(state = emptyListState, dispatcher = store::dispatch)
            return@Scaffold
        }

        LazyColumn(
            modifier = Modifier
                .padding(paddingValues)
                .padding(vertical = 16.dp),
        ) {
            items(state.bookmarkItems) { item ->
                var showMenu by remember { mutableStateOf(false) }
                if (state.isGuidMarkedForDeletion(item.guid)) {
                    return@items
                }

                when (item) {
                    is BookmarkItem.Bookmark -> {
                        SelectableFaviconListItem(
                            label = item.title,
                            url = item.previewImageUrl,
                            isSelected = item in state.selectedItems,
                            description = item.url,
                            onClick = { store.dispatch(BookmarkClicked(item)) },
                            onLongClick = { store.dispatch(BookmarkLongClicked(item)) },
                        ) {
                            Box {
                                IconButton(
                                    onClick = { showMenu = true },
                                    modifier = Modifier.size(24.dp),
                                ) {
                                    Icon(
                                        painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                                        contentDescription = stringResource(
                                            R.string.bookmark_item_menu_button_content_description,
                                            item.title,
                                        ),
                                        tint = FirefoxTheme.colors.iconPrimary,
                                    )
                                }

                                BookmarkListItemMenu(
                                    showMenu = showMenu,
                                    onDismissRequest = { showMenu = false },
                                    bookmark = item,
                                    store = store,
                                )
                            }
                        }
                    }

                    is BookmarkItem.Folder -> {
                        Box {
                            if (item.isDesktopFolder) {
                                SelectableIconListItem(
                                    label = item.title,
                                    isSelected = item in state.selectedItems,
                                    onClick = { store.dispatch(FolderClicked(item)) },
                                    beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                                )
                            } else {
                                SelectableIconListItem(
                                    label = item.title,
                                    isSelected = item in state.selectedItems,
                                    onClick = { store.dispatch(FolderClicked(item)) },
                                    onLongClick = { store.dispatch(FolderLongClicked(item)) },
                                    beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                                ) {
                                    Box {
                                        IconButton(
                                            onClick = { showMenu = true },
                                            modifier = Modifier.size(24.dp),
                                        ) {
                                            Icon(
                                                painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                                                contentDescription = stringResource(
                                                    R.string.bookmark_item_menu_button_content_description,
                                                    item.title,
                                                ),
                                                tint = FirefoxTheme.colors.iconPrimary,
                                            )
                                        }

                                        BookmarkListFolderMenu(
                                            showMenu = showMenu,
                                            onDismissRequest = { showMenu = false },
                                            folder = item,
                                            store = store,
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Suppress("LongMethod")
@Composable
private fun BookmarksListTopBar(
    store: BookmarksStore,
) {
    val selectedItems by store.observeAsState(store.state.selectedItems) { it.selectedItems }
    val recursiveCount by store.observeAsState(store.state.recursiveSelectedCount) { it.recursiveSelectedCount }
    val isCurrentFolderMobileRoot by store.observeAsState(store.state.currentFolder.isMobileRoot) {
        store.state.currentFolder.isMobileRoot
    }
    val isCurrentFolderDesktopRoot by store.observeAsState(store.state.currentFolder.isDesktopRoot) {
        store.state.currentFolder.isDesktopRoot
    }
    val folderTitle by store.observeAsState(store.state.currentFolder.title) { store.state.currentFolder.title }
    var showMenu by remember { mutableStateOf(false) }

    val backgroundColor = if (selectedItems.isEmpty()) {
        FirefoxTheme.colors.layer1
    } else {
        FirefoxTheme.colors.layerAccent
    }

    val textColor = if (selectedItems.isEmpty()) {
        FirefoxTheme.colors.textPrimary
    } else {
        FirefoxTheme.colors.textOnColorPrimary
    }

    val iconColor = if (selectedItems.isEmpty()) {
        FirefoxTheme.colors.textPrimary
    } else {
        FirefoxTheme.colors.iconOnColor
    }

    Box {
        TopAppBar(
            backgroundColor = backgroundColor,
            title = {
                Text(
                    color = textColor,
                    style = FirefoxTheme.typography.headline6,
                    text = if (selectedItems.isNotEmpty()) {
                        val total = selectedItems.size + (recursiveCount ?: 0)
                        stringResource(R.string.bookmarks_multi_select_title, total)
                    } else {
                        folderTitle
                    },
                )
            },
            navigationIcon = {
                IconButton(onClick = { store.dispatch(BackClicked) }) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_back_24),
                        contentDescription = stringResource(R.string.bookmark_navigate_back_button_content_description),
                        tint = iconColor,
                    )
                }
            },
            actions = {
                when {
                    selectedItems.isEmpty() -> {
                        if (isCurrentFolderDesktopRoot) {
                            Unit
                        } else {
                            IconButton(onClick = { store.dispatch(AddFolderClicked) }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_folder_add_24),
                                    contentDescription = stringResource(
                                        R.string.bookmark_select_folder_new_folder_button_title,
                                    ),
                                    tint = iconColor,
                                )
                            }
                        }

                        if (!isCurrentFolderMobileRoot) {
                            IconButton(onClick = { store.dispatch(CloseClicked) }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_cross_24),
                                    contentDescription = stringResource(
                                        R.string.content_description_close_button,
                                    ),
                                    tint = FirefoxTheme.colors.iconPrimary,
                                )
                            }
                        }
                    }
                    selectedItems.any { it is BookmarkItem.Folder } -> {
                        IconButton(onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.MoveClicked) }) {
                            Icon(
                                painter = painterResource(R.drawable.mozac_ic_folder_arrow_right_24),
                                contentDescription = stringResource(R.string.bookmark_menu_move_button),
                                tint = iconColor,
                            )
                        }

                        IconButton(onClick = {
                            store.dispatch(BookmarksListMenuAction.MultiSelect.DeleteClicked)
                        }) {
                            Icon(
                                painter = painterResource(R.drawable.mozac_ic_delete_24),
                                contentDescription = stringResource(R.string.bookmark_menu_delete_button),
                                tint = iconColor,
                            )
                        }
                    }
                    else -> {
                        if (selectedItems.size == 1) {
                            IconButton(onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.EditClicked) }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_edit_24),
                                    contentDescription = stringResource(R.string.bookmark_menu_edit_button),
                                    tint = iconColor,
                                )
                            }
                        }
                        IconButton(onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.MoveClicked) }) {
                            Icon(
                                painter = painterResource(R.drawable.mozac_ic_folder_arrow_right_24),
                                contentDescription = stringResource(R.string.bookmark_menu_move_button),
                                tint = iconColor,
                            )
                        }
                        Box {
                            IconButton(onClick = { showMenu = true }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                                    contentDescription = stringResource(
                                        R.string.content_description_menu,
                                    ),
                                    tint = iconColor,
                                )
                            }

                            BookmarkListOverflowMenu(
                                showMenu = showMenu,
                                onDismissRequest = { showMenu = false },
                                store = store,
                            )
                        }
                    }
                }
            },
        )
    }
}

@Composable
private fun WarnDialog(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state.openTabsConfirmationDialog) {
        it.openTabsConfirmationDialog
    }

    val dialog = state
    if (dialog is OpenTabsConfirmationDialog.Presenting) {
        AlertDialog(
            title = {
                Text(
                    text = stringResource(R.string.open_all_warning_title, dialog.numberOfTabs),
                    color = FirefoxTheme.colors.textPrimary,
                )
            },
            text = {
                Text(
                    text = stringResource(R.string.open_all_warning_message, dialog.numberOfTabs),
                    color = FirefoxTheme.colors.textPrimary,
                )
            },
            onDismissRequest = { store.dispatch(OpenTabsConfirmationDialogAction.CancelTapped) },
            confirmButton = {
                TextButton(
                    onClick = { store.dispatch(OpenTabsConfirmationDialogAction.ConfirmTapped) },
                ) {
                    Text(
                        text = stringResource(R.string.open_all_warning_confirm),
                        color = FirefoxTheme.colors.actionPrimary,
                    )
                }
            },
            dismissButton = {
                TextButton(
                    onClick = { store.dispatch(OpenTabsConfirmationDialogAction.CancelTapped) },
                ) {
                    Text(
                        text = stringResource(R.string.open_all_warning_cancel),
                        color = FirefoxTheme.colors.actionPrimary,
                    )
                }
            },
            backgroundColor = FirefoxTheme.colors.layer2,
        )
    }
}

@Composable
private fun AlertDialogDeletionWarning(
    onCancelTapped: () -> Unit,
    onDeleteTapped: () -> Unit,
) {
    AlertDialog(
        title = {
            Text(
                text = stringResource(R.string.bookmark_delete_folders_confirmation_dialog),
                color = FirefoxTheme.colors.textPrimary,
            )
        },
        onDismissRequest = onCancelTapped,
        confirmButton = {
            TextButton(
                onClick = onDeleteTapped,
            ) {
                Text(
                    text = stringResource(R.string.bookmark_menu_delete_button).uppercase(),
                    color = FirefoxTheme.colors.textAccent,
                )
            }
        },
        dismissButton = {
            TextButton(
                onClick = onCancelTapped,
            ) {
                Text(
                    text = stringResource(R.string.bookmark_delete_negative).uppercase(),
                    color = FirefoxTheme.colors.textAccent,
                )
            }
        },
        backgroundColor = FirefoxTheme.colors.layer2,
    )
}

@Composable
private fun SelectFolderScreen(
    store: BookmarksStore,
) {
    val showNewFolderButton by store.observeAsState(store.state.showNewFolderButton) { store.state.showNewFolderButton }
    val state by store.observeAsState(store.state.bookmarksSelectFolderState) { it.bookmarksSelectFolderState }

    LaunchedEffect(Unit) {
        store.dispatch(SelectFolderAction.ViewAppeared)
    }

    Scaffold(
        topBar = {
            SelectFolderTopBar(
                onBackClick = { store.dispatch(BackClicked) },
                onNewFolderClick = if (showNewFolderButton) {
                    { store.dispatch(AddFolderClicked) }
                } else {
                    null
                },
            )
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        LazyColumn(
            modifier = Modifier
                .padding(paddingValues)
                .padding(vertical = 16.dp),
        ) {
            items(state?.folders ?: listOf()) { folder ->
                if (store.state.isGuidBeingMoved(folder.guid)) {
                    return@items
                }

                if (folder.isDesktopRoot) {
                    Row(modifier = Modifier.padding(start = (40 * folder.indentation).dp)) {
                        // We need to account for not having an icon
                        Spacer(modifier = Modifier.width(56.dp))
                        Text(
                            text = folder.title,
                            color = FirefoxTheme.colors.textAccent,
                            style = FirefoxTheme.typography.headline8,
                        )
                    }
                } else {
                    SelectableIconListItem(
                        label = folder.title,
                        isSelected = folder.guid == state?.selectedGuid,
                        onClick = { store.dispatch(SelectFolderAction.ItemClicked(folder)) },
                        beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                        modifier = Modifier.padding(start = (40 * folder.indentation).dp),
                    )
                }
            }
            if (showNewFolderButton) {
                item {
                    IconListItem(
                        label = stringResource(R.string.bookmark_select_folder_new_folder_button_title),
                        labelTextColor = FirefoxTheme.colors.textAccent,
                        beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_add_24),
                        beforeIconTint = FirefoxTheme.colors.textAccent,
                        onClick = { store.dispatch(AddFolderClicked) },
                    )
                }
            }
        }
    }
}

@Composable
private fun SelectFolderTopBar(
    onBackClick: () -> Unit,
    onNewFolderClick: (() -> Unit)?,
) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = stringResource(R.string.bookmark_select_folder_fragment_label),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            IconButton(onClick = onBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.bookmark_navigate_back_button_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
        actions = {
            if (onNewFolderClick != null) {
                IconButton(onClick = onNewFolderClick) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_folder_add_24),
                        contentDescription = stringResource(
                            R.string.bookmark_add_new_folder_button_content_description,
                        ),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            }
        },
    )
}

private sealed class EmptyListState {
    data object NotAuthenticated : EmptyListState()
    data object Authenticated : EmptyListState()
    data object Folder : EmptyListState()
}

private fun BookmarksState.emptyListState(): EmptyListState? {
    return when {
        bookmarkItems.isNotEmpty() -> null
        currentFolder.guid != BookmarkRoot.Mobile.id -> EmptyListState.Folder
        isSignedIntoSync -> EmptyListState.Authenticated
        !isSignedIntoSync -> EmptyListState.NotAuthenticated
        else -> null
    }
}

@DrawableRes
private fun EmptyListState.drawableId(): Int = when (this) {
    is EmptyListState.NotAuthenticated,
    EmptyListState.Authenticated,
    -> R.drawable.bookmarks_star_illustration
    EmptyListState.Folder -> R.drawable.bookmarks_folder_illustration
}

@StringRes
private fun EmptyListState.descriptionId(): Int = when (this) {
    is EmptyListState.NotAuthenticated -> R.string.bookmark_empty_list_guest_description
    EmptyListState.Authenticated -> R.string.bookmark_empty_list_authenticated_description
    EmptyListState.Folder -> R.string.bookmark_empty_list_folder_description
}

@Composable
private fun EmptyList(
    state: EmptyListState,
    dispatcher: (BookmarksAction) -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 32.dp),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Image(
                painter = painterResource(state.drawableId()),
                contentDescription = null,
            )
            Text(
                text = stringResource(R.string.bookmark_empty_list_title),
                style = FirefoxTheme.typography.headline7,
                color = FirefoxTheme.colors.textPrimary,
            )
            Text(
                text = stringResource(state.descriptionId()),
                style = FirefoxTheme.typography.body2,
                color = FirefoxTheme.colors.textPrimary,
                textAlign = TextAlign.Center,
            )
            if (state is EmptyListState.NotAuthenticated) {
                TextButton(
                    onClick = { dispatcher(SignIntoSyncClicked) },
                    colors = ButtonDefaults.buttonColors(backgroundColor = FirefoxTheme.colors.actionPrimary),
                    shape = RoundedCornerShape(4.dp),
                    modifier = Modifier
                        .heightIn(36.dp)
                        .fillMaxWidth(),
                ) {
                    Text(
                        text = stringResource(R.string.bookmark_empty_list_guest_cta),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.button,
                        textAlign = TextAlign.Center,
                    )
                }
            }
        }
    }
}

@Composable
@Suppress("Deprecation") // https://bugzilla.mozilla.org/show_bug.cgi?id=1927718
private fun BookmarkListOverflowMenu(
    showMenu: Boolean,
    onDismissRequest: () -> Unit,
    store: BookmarksStore,
) {
    val menuItems = listOf(
        MenuItem(
            title = stringResource(R.string.bookmark_menu_open_in_new_tab_button),
            onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.OpenInNormalTabsClicked) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_open_in_private_tab_button),
            onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_share_button),
            onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.ShareClicked) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_delete_button),
            color = FirefoxTheme.colors.actionCritical,
            onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.DeleteClicked) },
        ),
    )
    ContextualMenu(
        menuItems = menuItems,
        showMenu = showMenu,
        onDismissRequest = onDismissRequest,
    )
}

@Composable
@Suppress("Deprecation") // https://bugzilla.mozilla.org/show_bug.cgi?id=1927718
private fun BookmarkListItemMenu(
    showMenu: Boolean,
    onDismissRequest: () -> Unit,
    bookmark: BookmarkItem.Bookmark,
    store: BookmarksStore,
) {
    val menuItems = listOf(
        MenuItem(
            title = stringResource(R.string.bookmark_menu_edit_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Bookmark.EditClicked(bookmark)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_copy_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Bookmark.CopyClicked(bookmark)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_share_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Bookmark.ShareClicked(bookmark)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_open_in_new_tab_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Bookmark.OpenInNormalTabClicked(bookmark)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_open_in_private_tab_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Bookmark.OpenInPrivateTabClicked(bookmark)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_delete_button),
            color = FirefoxTheme.colors.actionCritical,
            onClick = { store.dispatch(BookmarksListMenuAction.Bookmark.DeleteClicked(bookmark)) },
        ),
    )
    ContextualMenu(
        menuItems = menuItems,
        showMenu = showMenu,
        onDismissRequest = onDismissRequest,
    )
}

@Composable
@Suppress("Deprecation") // https://bugzilla.mozilla.org/show_bug.cgi?id=1927718
private fun BookmarkListFolderMenu(
    showMenu: Boolean,
    onDismissRequest: () -> Unit,
    folder: BookmarkItem.Folder,
    store: BookmarksStore,
) {
    val menuItems = listOf(
        MenuItem(
            title = stringResource(R.string.bookmark_menu_edit_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Folder.EditClicked(folder)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_open_all_in_tabs_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Folder.OpenAllInNormalTabClicked(folder)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_open_all_in_private_tabs_button),
            onClick = { store.dispatch(BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked(folder)) },
        ),
        MenuItem(
            title = stringResource(R.string.bookmark_menu_delete_button),
            color = FirefoxTheme.colors.actionCritical,
            onClick = { store.dispatch(BookmarksListMenuAction.Folder.DeleteClicked(folder)) },
        ),
    )
    ContextualMenu(
        menuItems = menuItems,
        showMenu = showMenu,
        onDismissRequest = onDismissRequest,
    )
}

@Composable
private fun EditFolderScreen(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state) { it }
    val editState = state.bookmarksEditFolderState
    val dialogState = state.bookmarksDeletionDialogState

    if (dialogState is DeletionDialogState.Presenting) {
        AlertDialogDeletionWarning(
            onCancelTapped = { store.dispatch(DeletionDialogAction.CancelTapped) },
            onDeleteTapped = { store.dispatch(DeletionDialogAction.DeleteTapped) },
        )
    }

    Scaffold(
        topBar = {
            EditFolderTopBar(
                onBackClick = { store.dispatch(BackClicked) },
                onDeleteClick = { store.dispatch(EditFolderAction.DeleteClicked) },
            )
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(modifier = Modifier.padding(paddingValues)) {
            TextField(
                value = editState?.folder?.title ?: "",
                onValueChange = { newText -> store.dispatch(EditFolderAction.TitleChanged(newText)) },
                placeholder = "",
                errorText = "",
                modifier = Modifier.padding(
                    start = 16.dp,
                    end = 16.dp,
                    top = 32.dp,
                ),
                label = stringResource(R.string.bookmark_name_label_normal_case),
            )

            Spacer(modifier = Modifier.height(24.dp))

            Text(
                stringResource(R.string.bookmark_save_in_label),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.body2,
                modifier = Modifier.padding(start = 16.dp),
            )

            IconListItem(
                label = editState?.parent?.title ?: "",
                beforeIconPainter = painterResource(R.drawable.ic_folder_icon),
                onClick = { store.dispatch(EditFolderAction.ParentFolderClicked) },
            )
        }
    }
}

@Composable
private fun EditFolderTopBar(
    onBackClick: () -> Unit,
    onDeleteClick: () -> Unit,
) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = stringResource(R.string.edit_bookmark_folder_fragment_title),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            IconButton(onClick = onBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.bookmark_navigate_back_button_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
        actions = {
            IconButton(onClick = onDeleteClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_delete_24),
                    contentDescription = stringResource(R.string.bookmark_delete_folder_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
    )
}

@Composable
private fun AddFolderScreen(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state.bookmarksAddFolderState) { it.bookmarksAddFolderState }
    Scaffold(
        topBar = { AddFolderTopBar(onBackClick = { store.dispatch(BackClicked) }) },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(modifier = Modifier.padding(paddingValues)) {
            TextField(
                value = state?.folderBeingAddedTitle ?: "",
                onValueChange = { newText -> store.dispatch(AddFolderAction.TitleChanged(newText)) },
                placeholder = "",
                errorText = "",
                modifier = Modifier.padding(
                    start = 16.dp,
                    end = 16.dp,
                    top = 32.dp,
                ),
                label = stringResource(R.string.bookmark_name_label_normal_case),
            )

            Spacer(modifier = Modifier.height(24.dp))

            Text(
                stringResource(R.string.bookmark_save_in_label),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.body2,
                modifier = Modifier.padding(start = 16.dp),
            )

            IconListItem(
                label = state?.parent?.title ?: "",
                beforeIconPainter = painterResource(R.drawable.ic_folder_icon),
                onClick = { store.dispatch(AddFolderAction.ParentFolderClicked) },
            )
        }
    }
}

@Composable
private fun AddFolderTopBar(onBackClick: () -> Unit) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = stringResource(R.string.bookmark_add_folder),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            IconButton(onClick = onBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.bookmark_navigate_back_button_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
    )
}

@Composable
private fun EditBookmarkScreen(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state.bookmarksEditBookmarkState) { it.bookmarksEditBookmarkState }

    val bookmark = state?.bookmark ?: return
    val folder = state?.folder ?: return

    Scaffold(
        topBar = {
            EditBookmarkTopBar(
                onBackClick = { store.dispatch(BackClicked) },
                onDeleteClicked = { store.dispatch(EditBookmarkAction.DeleteClicked) },
            )
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .padding(paddingValues)
                .padding(16.dp),
        ) {
            BookmarkEditor(
                bookmarkItem = bookmark,
                onTitleChanged = { store.dispatch(EditBookmarkAction.TitleChanged(it)) },
                onURLChanged = { store.dispatch(EditBookmarkAction.URLChanged(it)) },
            )
            Spacer(Modifier.height(24.dp))
            FolderInfo(
                folderTitle = folder.title,
                onFolderClicked = { store.dispatch(EditBookmarkAction.FolderClicked) },
            )
        }
    }
}

@Composable
private fun BookmarkEditor(
    bookmarkItem: BookmarkItem.Bookmark,
    onTitleChanged: (String) -> Unit,
    onURLChanged: (String) -> Unit,
) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Favicon(url = bookmarkItem.previewImageUrl, size = 64.dp)

        Column {
            ClearableTextField(
                value = bookmarkItem.title,
                onValueChange = onTitleChanged,
                placeholder = stringResource(R.string.bookmark_name_label_normal_case),
            )

            Spacer(modifier = Modifier.height(16.dp))

            ClearableTextField(
                value = bookmarkItem.url,
                onValueChange = onURLChanged,
                placeholder = stringResource(R.string.bookmark_url_label),
            )
        }
    }
}

@Composable
private fun FolderInfo(
    folderTitle: String,
    onFolderClicked: () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
        Text(
            text = "Save In",
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )

        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .height(40.dp)
                .fillMaxWidth()
                .clickable { onFolderClicked() },
        ) {
            Icon(
                painter = painterResource(id = iconsR.drawable.mozac_ic_folder_24),
                contentDescription = "",
                tint = FirefoxTheme.colors.textPrimary,
            )
            Text(
                text = folderTitle,
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.body2,
            )
        }
    }
}

@Composable
private fun ClearableTextField(
    placeholder: String,
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    var isFocused by remember { mutableStateOf(false) }

    TextField(
        value = value,
        onValueChange = onValueChange,
        placeholder = placeholder,
        errorText = "",
        modifier = modifier
            .onFocusChanged { isFocused = it.isFocused }
            .padding(0.dp)
            .paddingFromBaseline(0.dp),
        trailingIcon = {
            if (isFocused && value.isNotEmpty()) {
                IconButton(onClick = { onValueChange("") }) {
                    Icon(
                        painter = painterResource(id = iconsR.drawable.mozac_ic_cross_circle_fill_24),
                        contentDescription = null,
                        tint = FirefoxTheme.colors.textPrimary,
                    )
                }
            }
        },
        trailingIconHeight = 48.dp,
        colors = TextFieldColors.default(
            placeholderColor = FirefoxTheme.colors.textPrimary,
        ),
    )
}

@Composable
private fun EditBookmarkTopBar(
    onBackClick: () -> Unit,
    onDeleteClicked: () -> Unit,
) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = stringResource(R.string.edit_bookmark_fragment_title),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            IconButton(onClick = onBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.bookmark_navigate_back_button_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
        actions = {
            IconButton(onClick = onDeleteClicked) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_delete_24),
                    contentDescription = stringResource(R.string.bookmark_delete_bookmark_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
    )
}

@Composable
@FlexibleWindowLightDarkPreview
private fun EditBookmarkScreenPreview() {
    val store = BookmarksStore(
        initialState = BookmarksState(
            bookmarkItems = listOf(),
            selectedItems = listOf(),
            recursiveSelectedCount = null,
            currentFolder = BookmarkItem.Folder(
                guid = BookmarkRoot.Mobile.id,
                title = "Bookmarks",
            ),
            isSignedIntoSync = true,
            openTabsConfirmationDialog = OpenTabsConfirmationDialog.None,
            bookmarksDeletionDialogState = DeletionDialogState.None,
            bookmarksSnackbarState = BookmarksSnackbarState.None,
            bookmarksAddFolderState = null,
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = BookmarkItem.Bookmark(
                    url = "https://www.whoevenmakeswebaddressesthislonglikeseriously1.com",
                    title = "this is a very long bookmark title that should overflow 1",
                    previewImageUrl = "",
                    guid = "1",
                ),
                folder = BookmarkItem.Folder("folder 1", guid = "1"),
            ),
            bookmarksSelectFolderState = null,
            bookmarksEditFolderState = null,
            bookmarksMultiselectMoveState = null,
        ),
    )

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            EditBookmarkScreen(store = store)
        }
    }
}

@Composable
@FlexibleWindowLightDarkPreview
@Suppress("MagicNumber")
private fun BookmarksScreenPreview() {
    val bookmarkItems = List(20) {
        if (it % 2 == 0) {
            BookmarkItem.Bookmark(
                url = "https://www.whoevenmakeswebaddressesthislonglikeseriously$it.com",
                title = "this is a very long bookmark title that should overflow $it",
                previewImageUrl = "",
                guid = "$it",
            )
        } else {
            BookmarkItem.Folder("folder $it", guid = "$it")
        }
    }

    val store = { _: NavHostController ->
        BookmarksStore(
            initialState = BookmarksState(
                bookmarkItems = bookmarkItems,
                selectedItems = listOf(),
                recursiveSelectedCount = null,
                currentFolder = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                isSignedIntoSync = false,
                openTabsConfirmationDialog = OpenTabsConfirmationDialog.None,
                bookmarksDeletionDialogState = DeletionDialogState.None,
                bookmarksSnackbarState = BookmarksSnackbarState.None,
                bookmarksAddFolderState = null,
                bookmarksEditBookmarkState = null,
                bookmarksSelectFolderState = null,
                bookmarksEditFolderState = null,
                bookmarksMultiselectMoveState = null,
            ),
        )
    }

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            BookmarksScreen(buildStore = store)
        }
    }
}

@Composable
@FlexibleWindowLightDarkPreview
private fun EmptyBookmarksScreenPreview() {
    val store = { _: NavHostController ->
        BookmarksStore(
            initialState = BookmarksState(
                bookmarkItems = listOf(),
                selectedItems = listOf(),
                recursiveSelectedCount = null,
                currentFolder = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                isSignedIntoSync = false,
                openTabsConfirmationDialog = OpenTabsConfirmationDialog.None,
                bookmarksDeletionDialogState = DeletionDialogState.None,
                bookmarksSnackbarState = BookmarksSnackbarState.None,
                bookmarksAddFolderState = null,
                bookmarksEditBookmarkState = null,
                bookmarksSelectFolderState = null,
                bookmarksEditFolderState = null,
                bookmarksMultiselectMoveState = null,
            ),
        )
    }

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            BookmarksScreen(buildStore = store)
        }
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun AddFolderPreview() {
    val store = BookmarksStore(
        initialState = BookmarksState(
            bookmarkItems = listOf(),
            selectedItems = listOf(),
            recursiveSelectedCount = null,
            currentFolder = BookmarkItem.Folder(
                guid = BookmarkRoot.Mobile.id,
                title = "Bookmarks",
            ),
            isSignedIntoSync = false,
            openTabsConfirmationDialog = OpenTabsConfirmationDialog.None,
            bookmarksDeletionDialogState = DeletionDialogState.None,
            bookmarksSnackbarState = BookmarksSnackbarState.None,
            bookmarksEditBookmarkState = null,
            bookmarksAddFolderState = BookmarksAddFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folderBeingAddedTitle = "Edit me!",
            ),
            bookmarksSelectFolderState = null,
            bookmarksEditFolderState = null,
            bookmarksMultiselectMoveState = null,
        ),
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            AddFolderScreen(store)
        }
    }
}

@FlexibleWindowLightDarkPreview
@Composable
@Suppress("MagicNumber")
private fun SelectFolderPreview() {
    val store = BookmarksStore(
        initialState = BookmarksState(
            bookmarkItems = listOf(),
            selectedItems = listOf(),
            recursiveSelectedCount = null,
            currentFolder = BookmarkItem.Folder(
                guid = BookmarkRoot.Mobile.id,
                title = "Bookmarks",
            ),
            isSignedIntoSync = false,
            bookmarksEditBookmarkState = null,
            bookmarksAddFolderState = BookmarksAddFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folderBeingAddedTitle = "Edit me!",
            ),
            openTabsConfirmationDialog = OpenTabsConfirmationDialog.None,
            bookmarksDeletionDialogState = DeletionDialogState.None,
            bookmarksSnackbarState = BookmarksSnackbarState.None,
            bookmarksEditFolderState = null,
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = "",
                innerSelectionGuid = "guid1",
                folders = listOf(
                    SelectFolderItem(0, BookmarkItem.Folder("Bookmarks", "guid0")),
                    SelectFolderItem(1, BookmarkItem.Folder("Desktop Bookmarks", BookmarkRoot.Root.id)),
                    SelectFolderItem(2, BookmarkItem.Folder("Bookmarks Menu", BookmarkRoot.Menu.id)),
                    SelectFolderItem(2, BookmarkItem.Folder("Bookmarks Toolbar", BookmarkRoot.Toolbar.id)),
                    SelectFolderItem(2, BookmarkItem.Folder("Bookmarks Unfiled", BookmarkRoot.Unfiled.id)),
                    SelectFolderItem(1, BookmarkItem.Folder("Nested One", "guid0")),
                    SelectFolderItem(2, BookmarkItem.Folder("Nested Two", "guid0")),
                    SelectFolderItem(2, BookmarkItem.Folder("Nested Two", "guid0")),
                    SelectFolderItem(1, BookmarkItem.Folder("Nested One", "guid0")),
                    SelectFolderItem(2, BookmarkItem.Folder("Nested Two", "guid1")),
                    SelectFolderItem(3, BookmarkItem.Folder("Nested Three", "guid0")),
                    SelectFolderItem(0, BookmarkItem.Folder("Nested 0", "guid0")),
                ),
            ),
            bookmarksMultiselectMoveState = null,
        ),
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            SelectFolderScreen(store)
        }
    }
}
