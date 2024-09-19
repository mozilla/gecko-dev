/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.activity.compose.BackHandler
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.material.TextField
import androidx.compose.material.TextFieldDefaults
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.compose.list.SelectableFaviconListItem
import org.mozilla.fenix.compose.list.SelectableIconListItem
import org.mozilla.fenix.theme.FirefoxTheme

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
    }
}

internal object BookmarksDestinations {
    const val LIST = "list"
    const val ADD_FOLDER = "add folder"
}

/**
 * The Bookmarks list screen.
 */
@Suppress("LongParameterList")
@Composable
private fun BookmarksList(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state) { it }
    Scaffold(
        floatingActionButton = {
            FloatingActionButton(
                icon = painterResource(R.drawable.mozac_ic_search_24),
                contentDescription = stringResource(R.string.bookmark_search_button_content_description),
                onClick = { store.dispatch(SearchClicked) },
            )
        },
        topBar = {
            BookmarksListTopBar(
                folderTitle = state.folderTitle,
                onBackClick = { store.dispatch(BackClicked) },
                onNewFolderClick = { store.dispatch(AddFolderClicked) },
            )
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
                when (item) {
                    is BookmarkItem.Bookmark -> SelectableFaviconListItem(
                        label = item.title,
                        url = item.previewImageUrl,
                        isSelected = item in state.selectedItems,
                        description = item.url,
                        onClick = { store.dispatch(BookmarkClicked(item)) },
                        onLongClick = { store.dispatch(BookmarkLongClicked(item)) },
                        iconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                        onIconClick = { /* TODO show menu */ },
                        iconDescription = stringResource(
                            R.string.bookmark_item_menu_button_content_description,
                            item.title,
                        ),
                    )

                    is BookmarkItem.Folder -> SelectableIconListItem(
                        label = item.title,
                        isSelected = item in state.selectedItems,
                        onClick = { store.dispatch(FolderClicked(item)) },
                        onLongClick = { store.dispatch(FolderLongClicked(item)) },
                        beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                        afterIconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                        onAfterIconClick = { /* TODO show menu */ },
                        afterIconDescription = stringResource(
                            R.string.bookmark_item_menu_button_content_description,
                            item.title,
                        ),
                    )
                }
            }
        }
    }
}

@Composable
private fun BookmarksListTopBar(
    folderTitle: String,
    onBackClick: () -> Unit,
    onNewFolderClick: () -> Unit,
) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = folderTitle,
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
            IconButton(onClick = onNewFolderClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_folder_add_24),
                    contentDescription = stringResource(R.string.bookmark_add_new_folder_button_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
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
        folderGuid != BookmarkRoot.Mobile.id -> EmptyListState.Folder
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
private fun AddFolderScreen(
    store: BookmarksStore,
) {
    val state by store.observeAsState(store.state) { it }
    Scaffold(
        topBar = { AddFolderTopBar(onBackClick = { store.dispatch(BackClicked) }) },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(modifier = Modifier.padding(paddingValues)) {
            TextField(
                value = state.bookmarksAddFolderState?.folderBeingAddedTitle ?: "",
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
                label = state.folderTitle,
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
@FlexibleWindowLightDarkPreview
@Suppress("MagicNumber")
private fun BookmarksScreenPreview() {
    val bookmarkItems = List(10) {
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
                folderTitle = "Bookmarks",
                folderGuid = BookmarkRoot.Mobile.id,
                bookmarksAddFolderState = null,
                isSignedIntoSync = false,
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
@Suppress("MagicNumber")
private fun EmptyBookmarksScreenPreview() {
    val store = { _: NavHostController ->
        BookmarksStore(
            initialState = BookmarksState(
                bookmarkItems = listOf(),
                selectedItems = listOf(),
                folderTitle = "Bookmarks",
                folderGuid = BookmarkRoot.Mobile.id,
                bookmarksAddFolderState = null,
                isSignedIntoSync = false,
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
            folderTitle = "Bookmarks",
            folderGuid = BookmarkRoot.Mobile.id,
            bookmarksAddFolderState = BookmarksAddFolderState(
                folderBeingAddedTitle = "Edit me!",
            ),
            isSignedIntoSync = false,
        ),
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            AddFolderScreen(store)
        }
    }
}
