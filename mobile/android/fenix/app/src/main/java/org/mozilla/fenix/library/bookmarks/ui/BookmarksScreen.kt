/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TextField
import androidx.compose.material.TextFieldDefaults
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
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
    val state by store.observeAsState(initialValue = store.state) { it }
    BackHandler { store.dispatch(BackClicked) }
    NavHost(
        navController = navController,
        startDestination = BookmarksDestinations.LIST,
    ) {
        composable(route = BookmarksDestinations.LIST) {
            BookmarksList(
                bookmarkItems = state.bookmarkItems,
                selectedItems = state.selectedItems,
                folderTitle = state.folderTitle,
                onBookmarkClick = { item -> store.dispatch(BookmarkClicked(item)) },
                onBookmarkLongClick = { item -> store.dispatch(BookmarkLongClicked(item)) },
                onFolderClick = { item -> store.dispatch(FolderClicked(item)) },
                onFolderLongClick = { item -> store.dispatch(FolderLongClicked(item)) },
                onBackClick = { store.dispatch(BackClicked) },
                onNewFolderClick = { store.dispatch(AddFolderClicked) },
                onMenuClick = {},
                onSearchClick = { store.dispatch(SearchClicked) },
            )
        }
        composable(route = BookmarksDestinations.ADD_FOLDER) {
            AddFolderScreen(
                folderTitle = state.bookmarksAddFolderState?.folderBeingAddedTitle ?: "",
                parentFolderTitle = state.folderTitle,
                onTextChange = { updatedText -> store.dispatch(AddFolderAction.TitleChanged(updatedText)) },
                onParentFolderIconClick = { store.dispatch(AddFolderAction.ParentFolderClicked) },
                onBackClick = { store.dispatch(BackClicked) },
            )
        }
    }
}

internal object BookmarksDestinations {
    const val LIST = "list"
    const val ADD_FOLDER = "add folder"
}

/**
 * The Bookmarks list screen.
 * @param bookmarkItems Bookmarks and folders to display.
 * @param selectedItems The currently selected items in the list.
 * @param folderTitle The display title of the currently selected bookmarks folder.
 * @param onBookmarkClick Invoked when the user clicks on a bookmark item row.
 * @param onBookmarkLongClick Invoked when the user clicks on a bookmark item row.
 * @param onFolderClick Invoked when the user clicks on a folder item row.
 * @param onFolderLongClick Invoked when the user clicks on a folder item row.
 * @param onBackClick Invoked when the user clicks on the toolbar back button.
 * @param onNewFolderClick Invoked when the user clicks on the toolbar new folder button.
 * @param onMenuClick Invoked when the user clicks on a bookmark item overflow menu.
 * @param onSearchClick Invoked when the user clicks on the search floating action button.
 */
@Suppress("LongParameterList")
@Composable
private fun BookmarksList(
    bookmarkItems: List<BookmarkItem>,
    selectedItems: List<BookmarkItem>,
    folderTitle: String,
    onBookmarkClick: (BookmarkItem.Bookmark) -> Unit,
    onBookmarkLongClick: (BookmarkItem.Bookmark) -> Unit,
    onFolderClick: (BookmarkItem.Folder) -> Unit,
    onFolderLongClick: (BookmarkItem.Folder) -> Unit,
    onBackClick: () -> Unit,
    onNewFolderClick: () -> Unit,
    onMenuClick: (BookmarkItem) -> Unit,
    onSearchClick: () -> Unit,
) {
    Scaffold(
        floatingActionButton = {
            FloatingActionButton(
                icon = painterResource(R.drawable.mozac_ic_search_24),
                contentDescription = stringResource(R.string.bookmark_search_button_content_description),
                onClick = onSearchClick,
            )
        },
        topBar = {
            BookmarksListTopBar(
                folderTitle = folderTitle,
                onBackClick = onBackClick,
                onNewFolderClick = onNewFolderClick,
            )
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        LazyColumn(
            modifier = Modifier
                .padding(paddingValues)
                .padding(vertical = 16.dp),
        ) {
            items(bookmarkItems) { item ->
                when (item) {
                    is BookmarkItem.Bookmark -> SelectableFaviconListItem(
                        label = item.title,
                        url = item.previewImageUrl,
                        isSelected = item in selectedItems,
                        description = item.url,
                        onClick = { onBookmarkClick(item) },
                        onLongClick = { onBookmarkLongClick(item) },
                        iconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                        onIconClick = { onMenuClick(item) },
                        iconDescription = stringResource(
                            R.string.bookmark_item_menu_button_content_description,
                            item.title,
                        ),
                    )

                    is BookmarkItem.Folder -> {
                        SelectableIconListItem(
                            label = item.title,
                            isSelected = item in selectedItems,
                            onClick = { onFolderClick(item) },
                            onLongClick = { onFolderLongClick(item) },
                            beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                            afterIconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                            onAfterIconClick = { onMenuClick(item) },
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

@Composable
private fun AddFolderScreen(
    folderTitle: String,
    parentFolderTitle: String,
    onTextChange: (String) -> Unit,
    onParentFolderIconClick: () -> Unit,
    onBackClick: () -> Unit,
) {
    Scaffold(
        topBar = { AddFolderTopBar(onBackClick) },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(modifier = Modifier.padding(paddingValues)) {
            TextField(
                value = folderTitle,
                onValueChange = { newText -> onTextChange(newText) },
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
                label = parentFolderTitle,
                beforeIconPainter = painterResource(R.drawable.ic_folder_icon),
                onClick = onParentFolderIconClick,
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
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            AddFolderScreen("", "Bookmarks", {}, {}, {})
        }
    }
}
