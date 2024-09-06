/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

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
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
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
 */
@Composable
internal fun BookmarksScreen(store: BookmarksStore) {
    val state by store.observeAsState(initialValue = store.state) { it }
    BookmarksList(
        bookmarkItems = state.bookmarkItems,
        selectedItems = state.selectedItems,
        folderTitle = state.folderTitle,
        onBookmarkClick = { item -> store.dispatch(BookmarkClicked(item)) },
        onBookmarkLongClick = { item -> store.dispatch(BookmarkLongClicked(item)) },
        onFolderClick = { item -> store.dispatch(FolderClicked(item)) },
        onFolderLongClick = { item -> store.dispatch(FolderLongClicked(item)) },
        onBackClick = {},
        onNewFolderClick = {},
        onCloseClick = {},
        onMenuClick = {},
        onSearchClick = { store.dispatch(SearchClicked) },
    )
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
 * @param onCloseClick Invoked when the user clicks on the toolbar close button.
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
    onCloseClick: () -> Unit,
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
                onCloseClick = onCloseClick,
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
    onCloseClick: () -> Unit,
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

            IconButton(onClick = onCloseClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_cross_24),
                    contentDescription = stringResource(R.string.bookmark_close_button_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
    )
}

@Composable
private fun AddFolder(
    parentFolderTitle: String,
    onTextChange: (String) -> Unit,
    onParentFolderIconClick: () -> Unit,
    onBackClick: () -> Unit,
) {
    Scaffold(topBar = { AddFolderTopBar(onBackClick) }) { paddingValues ->
        var text by remember { mutableStateOf("") }

        Column(modifier = Modifier.padding(paddingValues)) {
            TextField(
                value = text,
                onValueChange = { newText ->
                    text = newText
                    onTextChange(newText)
                },
                label = {
                    Text(
                        stringResource(R.string.bookmark_name_label_normal_case),
                        color = FirefoxTheme.colors.textPrimary,
                    )
                },
                modifier = Modifier.padding(start = 16.dp, top = 32.dp),
            )

            Spacer(modifier = Modifier.height(24.dp))

            Text(
                stringResource(R.string.bookmark_save_in_label),
                fontSize = 14.sp,
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

    val store = BookmarksStore(
        initialState = BookmarksState(
            bookmarkItems = bookmarkItems,
            folderTitle = "Bookmarks",
            selectedItems = listOf(),
        ),
    )

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            BookmarksScreen(store)
        }
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun AddFolderPreview() {
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            AddFolder("Bookmarks", {}, {}, {})
        }
    }
}
