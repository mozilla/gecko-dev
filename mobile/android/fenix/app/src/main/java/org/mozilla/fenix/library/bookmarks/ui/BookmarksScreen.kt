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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.SnackbarDuration
import androidx.compose.material.SnackbarHost
import androidx.compose.material.SnackbarHostState
import androidx.compose.material.SnackbarResult
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.material.TextField
import androidx.compose.material.TextFieldDefaults
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
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
import org.mozilla.fenix.compose.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.compose.list.SelectableFaviconListItem
import org.mozilla.fenix.compose.list.SelectableIconListItem
import org.mozilla.fenix.theme.FirefoxTheme
import mozilla.components.ui.icons.R as iconsR

/**
 * The UI host for the Bookmarks list screen and related subscreens.
 *
 * @param buildStore A builder function to construct a [BookmarksStore] using the NavController that's local
 * to the nav graph for the Bookmarks view hierarchy.
 */
@Composable
internal fun BookmarksScreen(buildStore: (NavHostController) -> BookmarksStore) {
    val navController = rememberNavController()
    val store = buildStore(navController)
    BackHandler { store.dispatch(BackClicked) }
    NavHost(
        navController = navController,
        startDestination = BookmarksDestinations.LIST,
    ) {
        composable(route = BookmarksDestinations.LIST) {
            BookmarksList(store = store)
        }
        composable(route = BookmarksDestinations.ADD_FOLDER) {
            AddFolderScreen(store = store)
        }
        composable(route = BookmarksDestinations.EDIT_FOLDER) {
            EditFolderScreen(store = store)
        }
        composable(route = BookmarksDestinations.EDIT_BOOKMARK) {
            EditBookmarkScreen(store = store)
        }
        composable(route = BookmarksDestinations.SELECT_FOLDER) {
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
    val snackbarHostState = remember { SnackbarHostState() }

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

    LaunchedEffect(state.bookmarksSnackbarState) {
        when (state.bookmarksSnackbarState) {
            BookmarksSnackbarState.None -> return@LaunchedEffect
            is BookmarksSnackbarState.UndoDeletion -> scope.launch {
                val result = snackbarHostState.showSnackbar(
                    message = snackbarMessage,
                    actionLabel = snackbarActionLabel,
                    duration = SnackbarDuration.Short,
                )

                when (result) {
                    SnackbarResult.ActionPerformed -> store.dispatch(SnackbarAction.Undo)
                    SnackbarResult.Dismissed -> store.dispatch(SnackbarAction.Dismissed)
                    else -> {}
                }
            }
            BookmarksSnackbarState.CantEditDesktopFolders -> scope.launch {
                val result = snackbarHostState.showSnackbar(
                    message = snackbarMessage,
                    duration = SnackbarDuration.Short,
                )

                when (result) {
                    SnackbarResult.Dismissed -> store.dispatch(SnackbarAction.Dismissed)
                    else -> {}
                }
            }
        }
    }

    Scaffold(
        snackbarHost = {
            SnackbarHost(hostState = snackbarHostState)
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
                        Box {
                            SelectableFaviconListItem(
                                label = item.title,
                                url = item.previewImageUrl,
                                isSelected = item in state.selectedItems,
                                description = item.url,
                                onClick = { store.dispatch(BookmarkClicked(item)) },
                                onLongClick = { store.dispatch(BookmarkLongClicked(item)) },
                                iconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                                onIconClick = { showMenu = true },
                                iconDescription = stringResource(
                                    R.string.bookmark_item_menu_button_content_description,
                                    item.title,
                                ),
                            )

                            BookmarkListItemMenu(
                                showMenu = showMenu,
                                onDismissRequest = { showMenu = false },
                                bookmark = item,
                                store = store,
                            )
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
                                    afterIconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                                    onAfterIconClick = { showMenu = true },
                                    afterIconDescription = stringResource(
                                        R.string.bookmark_item_menu_button_content_description,
                                        item.title,
                                    ),
                                )

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

@Suppress("LongMethod")
@Composable
private fun BookmarksListTopBar(
    store: BookmarksStore,
) {
    val selectedItems by store.observeAsState(store.state.selectedItems) { it.selectedItems }
    val isCurrentFolderDesktopRoot by store.observeAsState(store.state.currentFolder.isDesktopRoot) {
        store.state.currentFolder.isDesktopRoot
    }
    var showMenu by remember { mutableStateOf(false) }
    Box {
        BookmarkListOverflowMenu(
            showMenu = showMenu,
            onDismissRequest = { showMenu = false },
            store = store,
        )
        TopAppBar(
            backgroundColor = FirefoxTheme.colors.layer1,
            title = {
                Text(
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline6,
                    text = if (selectedItems.isNotEmpty()) {
                        stringResource(R.string.bookmarks_multi_select_title, selectedItems.size)
                    } else {
                        store.state.currentFolder.title
                    },
                )
            },
            navigationIcon = {
                IconButton(onClick = { store.dispatch(BackClicked) }) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_back_24),
                        contentDescription = stringResource(R.string.bookmark_navigate_back_button_content_description),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            },
            actions = {
                when (selectedItems.size) {
                    0 -> {
                        if (isCurrentFolderDesktopRoot) {
                            Unit
                        } else {
                            IconButton(onClick = { store.dispatch(AddFolderClicked) }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_folder_add_24),
                                    contentDescription = stringResource(
                                        R.string.bookmark_add_new_folder_button_content_description,
                                    ),
                                    tint = FirefoxTheme.colors.iconPrimary,
                                )
                            }
                        }
                    }

                    1 -> {
                        IconButton(onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.EditClicked) }) {
                            Icon(
                                painter = painterResource(R.drawable.mozac_ic_edit_24),
                                contentDescription = stringResource(R.string.bookmark_menu_edit_button),
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                        }
                        IconButton(onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.MoveClicked) }) {
                            Icon(
                                painter = painterResource(R.drawable.mozac_ic_move_24),
                                contentDescription = stringResource(R.string.bookmark_menu_move_button),
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                        }
                        if (selectedItems.none { it is BookmarkItem.Folder }) {
                            IconButton(onClick = { showMenu = true }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                                    contentDescription = stringResource(
                                        R.string.bookmark_selected_menu_button_content_description,
                                    ),
                                    tint = FirefoxTheme.colors.iconPrimary,
                                )
                            }
                        }
                    }

                    else -> {
                        IconButton(onClick = { store.dispatch(BookmarksListMenuAction.MultiSelect.MoveClicked) }) {
                            Icon(
                                painter = painterResource(R.drawable.mozac_ic_move_24),
                                contentDescription = stringResource(R.string.bookmark_menu_move_button),
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                        }
                        if (selectedItems.any { it is BookmarkItem.Folder }) {
                            IconButton(onClick = {
                                store.dispatch(BookmarksListMenuAction.MultiSelect.DeleteClicked)
                            }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_delete_24),
                                    contentDescription = stringResource(R.string.bookmark_menu_delete_button),
                                    tint = FirefoxTheme.colors.iconPrimary,
                                )
                            }
                        } else {
                            IconButton(onClick = { showMenu = true }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                                    contentDescription = stringResource(
                                        R.string.bookmark_selected_menu_button_content_description,
                                    ),
                                    tint = FirefoxTheme.colors.iconPrimary,
                                )
                            }
                        }
                    }
                }
            },
        )
    }
}

@Composable
private fun SelectFolderScreen(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state.bookmarksSelectFolderState) { it.bookmarksSelectFolderState }

    LaunchedEffect(Unit) {
        store.dispatch(SelectFolderAction.ViewAppeared)
    }

    Scaffold(
        topBar = {
            SelectFolderTopBar(
                onBackClick = { store.dispatch(BackClicked) },
                onNewFolderClick = if (state?.showNewFolderButton == true) {
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
            .padding(horizontal = 16.dp),
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
    val state by store.observeAsState(store.state.bookmarksEditFolderState) { it.bookmarksEditFolderState }
    Scaffold(
        topBar = { EditFolderTopBar(onBackClick = { store.dispatch(BackClicked) }) },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(modifier = Modifier.padding(paddingValues)) {
            TextField(
                value = state?.folder?.title ?: "",
                onValueChange = { newText -> store.dispatch(EditFolderAction.TitleChanged(newText)) },
                label = {
                    Text(
                        stringResource(R.string.bookmark_name_label_normal_case),
                        color = FirefoxTheme.colors.textPrimary,
                    )
                },
                colors = TextFieldDefaults.textFieldColors(textColor = FirefoxTheme.colors.textPrimary),
                modifier = Modifier.padding(start = 16.dp, top = 32.dp),
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
                onClick = { store.dispatch(EditFolderAction.ParentFolderClicked) },
            )
        }
    }
}

@Composable
private fun EditFolderTopBar(onBackClick: () -> Unit) {
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
                label = {
                    Text(
                        stringResource(R.string.bookmark_name_label_normal_case),
                        color = FirefoxTheme.colors.textPrimary,
                    )
                },
                colors = TextFieldDefaults.textFieldColors(textColor = FirefoxTheme.colors.textPrimary),
                modifier = Modifier.padding(start = 16.dp, top = 32.dp),
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

    LaunchedEffect(Unit) {
        // If we somehow get to this screen without a `bookmarksEditBookmarkState`
        // we'll want to navigate them back.
        if (state == null) {
            store.dispatch(BackClicked)
        }
    }

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
                .padding(horizontal = 16.dp),
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
            )
            ClearableTextField(
                value = bookmarkItem.url,
                onValueChange = onURLChanged,
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
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    var isFocused by remember { mutableStateOf(false) }

    TextField(
        value = value,
        onValueChange = onValueChange,
        singleLine = true,
        trailingIcon = {
            if (isFocused && value.isNotEmpty()) {
                IconButton(onClick = { onValueChange("") }) {
                    Icon(
                        painter = painterResource(id = iconsR.drawable.mozac_ic_cross_circle_fill_20),
                        contentDescription = null,
                        tint = FirefoxTheme.colors.textPrimary,
                    )
                }
            }
        },
        modifier = modifier
            .onFocusChanged { isFocused = it.isFocused }
            .padding(0.dp)
            .paddingFromBaseline(0.dp),
        colors = TextFieldDefaults.textFieldColors(
            backgroundColor = FirefoxTheme.colors.layer1,
            textColor = FirefoxTheme.colors.textPrimary,
            unfocusedIndicatorColor = FirefoxTheme.colors.textPrimary,
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
                    contentDescription = stringResource(R.string.bookmark_add_new_folder_button_content_description),
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
            currentFolder = BookmarkItem.Folder(
                guid = BookmarkRoot.Mobile.id,
                title = "Bookmarks",
            ),
            isSignedIntoSync = true,
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
                currentFolder = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                isSignedIntoSync = false,
                bookmarksSnackbarState = BookmarksSnackbarState.None,
                bookmarksAddFolderState = null,
                bookmarksEditBookmarkState = null,
                bookmarksSelectFolderState = null,
                bookmarksEditFolderState = null,
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
                currentFolder = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                isSignedIntoSync = false,
                bookmarksSnackbarState = BookmarksSnackbarState.None,
                bookmarksAddFolderState = null,
                bookmarksEditBookmarkState = null,
                bookmarksSelectFolderState = null,
                bookmarksEditFolderState = null,
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
            currentFolder = BookmarkItem.Folder(
                guid = BookmarkRoot.Mobile.id,
                title = "Bookmarks",
            ),
            isSignedIntoSync = false,
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
            bookmarksSnackbarState = BookmarksSnackbarState.None,
            bookmarksEditFolderState = null,
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                selectionGuid = null,
                folderSelectionGuid = "guid1",
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
        ),
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            SelectFolderScreen(store)
        }
    }
}
