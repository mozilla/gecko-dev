/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.compose.list.FaviconListItem
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The UI host for the Bookmarks list screen and related subscreens.
 */
@Composable
@Suppress("Unused")
fun BookmarksScreen() {
    BookmarksList(
        bookmarkItems = listOf(),
        onBookmarkItemClick = {},
        onBackClick = {},
        onNewFolderClick = {},
        onCloseClick = {},
        onMenuClick = {},
        onSearchClick = {},
    )
}

/**
 * The Bookmarks list screen.
 * @param bookmarkItems Bookmarks and folders to display.
 * @param onBookmarkItemClick Invoked when the user clicks on a bookmark item row.
 * @param onBackClick Invoked when the user clicks on the toolbar back button.
 * @param onNewFolderClick Invoked when the user clicks on the toolbar new folder button.
 * @param onCloseClick Invoked when the user clicks on the toolbar close button.
 * @param onMenuClick Invoked when the user clicks on a bookmark item overflow menu.
 * @param onSearchClick Invoked when the user clicks on the search floating action button.
 */
@Composable
private fun BookmarksList(
    bookmarkItems: List<BookmarkItem>,
    onBookmarkItemClick: (BookmarkItem) -> Unit,
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
                onBackClick = onBackClick,
                onNewFolderClick = onNewFolderClick,
                onCloseClick = onCloseClick,
            )
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        LazyColumn(modifier = Modifier.padding(paddingValues).padding(vertical = 16.dp)) {
            items(bookmarkItems) { item ->
                when (item) {
                    is BookmarkItem.Bookmark -> FaviconListItem(
                        label = item.title,
                        url = item.previewImageUrl,
                        description = item.url,
                        onClick = { onBookmarkItemClick(item) },
                        iconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                        onIconClick = { onMenuClick(item) },
                        iconDescription = stringResource(
                            R.string.bookmark_item_menu_button_content_description,
                            item.title,
                        ),
                    )

                    is BookmarkItem.Folder -> {
                        IconListItem(
                            label = item.name,
                            onClick = { onBookmarkItemClick(item) },
                            beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                            afterIconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                            onAfterIconClick = { onMenuClick(item) },
                            afterIconDescription = stringResource(
                                R.string.bookmark_item_menu_button_content_description,
                                item.name,
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
    onBackClick: () -> Unit,
    onNewFolderClick: () -> Unit,
    onCloseClick: () -> Unit,
) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = stringResource(R.string.library_bookmarks),
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
@FlexibleWindowLightDarkPreview
@Suppress("MagicNumber")
private fun BookmarksListPreview() {
    val bookmarks = List(10) {
        if (it % 2 == 0) {
            BookmarkItem.Bookmark(
                url = "https://www.whoevenmakeswebaddressesthislonglikeseriously$it.com",
                title = "this is a very long bookmark title that should overflow $it",
                previewImageUrl = "",
            )
        } else {
            BookmarkItem.Folder("folder $it")
        }
    }

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            BookmarksList(
                bookmarkItems = bookmarks,
                onBookmarkItemClick = {},
                onBackClick = {},
                onNewFolderClick = {},
                onCloseClick = {},
                onMenuClick = {},
                onSearchClick = {},
            )
        }
    }
}
