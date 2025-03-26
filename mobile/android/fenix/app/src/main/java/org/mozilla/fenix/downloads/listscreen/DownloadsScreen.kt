/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ChipDefaults
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.FilterChip
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ext.isItemPartiallyVisible
import org.mozilla.fenix.compose.list.ExpandableListHeader
import org.mozilla.fenix.compose.list.SelectableListItem
import org.mozilla.fenix.compose.menu.DropdownMenu
import org.mozilla.fenix.compose.menu.MenuItem
import org.mozilla.fenix.compose.snackbar.AcornSnackbarHostState
import org.mozilla.fenix.compose.snackbar.SnackbarHost
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.compose.text.Text
import org.mozilla.fenix.downloads.listscreen.store.CreatedTime
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIStore
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.downloads.listscreen.store.HeaderItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Downloads screen that displays the list of downloads.
 *
 * @param downloadsStore The [DownloadUIStore] used to manage and access the state of download items.
 * @param onItemClick Invoked when a download item is clicked.
 * @param onItemDeleteClick Invoked when delete icon button is clicked.
 */
@Composable
fun DownloadsScreen(
    downloadsStore: DownloadUIStore,
    onItemClick: (FileItem) -> Unit,
    onItemDeleteClick: (FileItem) -> Unit,
) {
    val uiState by downloadsStore.observeAsState(initialValue = downloadsStore.state) { it }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer1)
            .widthIn(max = FirefoxTheme.layout.size.containerMaxWidth),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        if (uiState.filtersToDisplay.isNotEmpty()) {
            Filters(
                selectedContentTypeFilter = uiState.selectedContentTypeFilter,
                contentTypeFilters = uiState.filtersToDisplay,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = FirefoxTheme.layout.space.static200),
                onContentTypeSelected = {
                    downloadsStore.dispatch(DownloadUIAction.ContentTypeSelected(it))
                },
            )
        }

        if (uiState.itemsToDisplay.isEmpty()) {
            EmptyState(modifier = Modifier.fillMaxSize())
        } else {
            DownloadsContent(
                state = uiState,
                onClick = onItemClick,
                onSelectionChange = { item, isSelected ->
                    if (isSelected) {
                        downloadsStore.dispatch(DownloadUIAction.AddItemForRemoval(item))
                    } else {
                        downloadsStore.dispatch(DownloadUIAction.RemoveItemForRemoval(item))
                    }
                },
                onDeleteClick = onItemDeleteClick,
                onShareUrlClick = { downloadsStore.dispatch(DownloadUIAction.ShareUrlClicked(it.url)) },
                onShareFileClick = {
                    downloadsStore.dispatch(
                        DownloadUIAction.ShareFileClicked(
                            it.filePath,
                            it.contentType,
                        ),
                    )
                },
                modifier = Modifier.fillMaxHeight(),
            )
        }
    }
}

@Composable
@OptIn(ExperimentalFoundationApi::class)
private fun DownloadsContent(
    state: DownloadUIState,
    modifier: Modifier = Modifier,
    onClick: (FileItem) -> Unit,
    onSelectionChange: (FileItem, Boolean) -> Unit,
    onDeleteClick: (FileItem) -> Unit,
    onShareUrlClick: (FileItem) -> Unit,
    onShareFileClick: (FileItem) -> Unit,
) {
    val haptics = LocalHapticFeedback.current

    LazyColumn(
        modifier = modifier,
    ) {
        itemsIndexed(
            items = state.itemsToDisplay,
            contentType = { _, item -> item::class },
            key = { _, item ->
                when (item) {
                    is HeaderItem -> item.createdTime
                    is FileItem -> item.id
                }
            },
        ) { index, listItem ->
            when (listItem) {
                is HeaderItem -> {
                    HeaderListItem(
                        headerItem = listItem,
                        modifier = Modifier.animateItem(),
                    )
                }

                is FileItem -> {
                    FileListItem(
                        fileItem = listItem,
                        isSelected = state.mode.selectedItems.contains(listItem),
                        isMenuIconVisible = state.isNormalMode,
                        onDeleteClick = onDeleteClick,
                        onShareUrlClick = onShareUrlClick,
                        onShareFileClick = onShareFileClick,
                        modifier = modifier
                            .animateItem()
                            .combinedClickable(
                                onClick = {
                                    if (state.isNormalMode) {
                                        onClick(listItem)
                                    } else {
                                        onSelectionChange(
                                            listItem,
                                            !state.mode.selectedItems.contains(listItem),
                                        )
                                    }
                                },
                                onLongClick = {
                                    if (state.isNormalMode) {
                                        haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                                        onSelectionChange(listItem, true)
                                    }
                                },
                            )
                            .testTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM}.${listItem.fileName}"),
                    )

                    if (index == state.itemsToDisplay.lastIndex || state.itemsToDisplay[index + 1] is HeaderItem) {
                        Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.static200))
                    }
                }
            }
        }
    }
}

@Composable
private fun FileListItem(
    fileItem: FileItem,
    isSelected: Boolean,
    isMenuIconVisible: Boolean,
    modifier: Modifier = Modifier,
    onDeleteClick: (FileItem) -> Unit,
    onShareUrlClick: (FileItem) -> Unit,
    onShareFileClick: (FileItem) -> Unit,
) {
    SelectableListItem(
        label = fileItem.fileName ?: fileItem.url,
        description = fileItem.description,
        isSelected = isSelected,
        icon = fileItem.icon,
        afterListAction = {
            if (isMenuIconVisible) {
                var menuExpanded by remember { mutableStateOf(false) }

                Spacer(modifier = Modifier.width(16.dp))

                IconButton(
                    onClick = { menuExpanded = true },
                    modifier = Modifier
                        .size(24.dp)
                        .testTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM_MENU}.${fileItem.fileName}"),
                ) {
                    Icon(
                        painter = painterResource(id = R.drawable.mozac_ic_ellipsis_vertical_24),
                        contentDescription = stringResource(id = R.string.content_description_menu),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )

                    DropdownMenu(
                        menuItems = listOf(
                            MenuItem.TextItem(
                                text = Text.Resource(R.string.download_share_url),
                                onClick = { onShareUrlClick(fileItem) },
                                level = MenuItem.FixedItem.Level.Default,
                            ),
                            MenuItem.TextItem(
                                text = Text.Resource(R.string.download_share_file),
                                onClick = { onShareFileClick(fileItem) },
                                level = MenuItem.FixedItem.Level.Default,
                            ),
                            MenuItem.TextItem(
                                text = Text.Resource(R.string.download_delete_item),
                                onClick = { onDeleteClick(fileItem) },
                                level = MenuItem.FixedItem.Level.Critical,
                            ),
                        ),
                        expanded = menuExpanded,
                        onDismissRequest = { menuExpanded = false },
                    )
                }
            }
        },
        modifier = modifier,
    )
}

@Composable
private fun HeaderListItem(
    headerItem: HeaderItem,
    modifier: Modifier = Modifier,
) {
    Box(modifier = modifier) {
        ExpandableListHeader(
            headerText = stringResource(id = headerItem.createdTime.stringRes),
        )
    }
}

@Composable
private fun Filters(
    selectedContentTypeFilter: FileItem.ContentTypeFilter,
    contentTypeFilters: List<FileItem.ContentTypeFilter>,
    modifier: Modifier = Modifier,
    onContentTypeSelected: (FileItem.ContentTypeFilter) -> Unit,
) {
    val listState = rememberLazyListState()
    LazyRow(
        modifier = modifier,
        state = listState,
        horizontalArrangement = Arrangement.spacedBy(FirefoxTheme.layout.space.static100),
        contentPadding = PaddingValues(horizontal = FirefoxTheme.layout.space.static200),
    ) {
        items(
            items = contentTypeFilters,
            key = { it },
        ) { contentTypeParam ->
            DownloadChip(
                selected = selectedContentTypeFilter == contentTypeParam,
                contentTypeFilter = contentTypeParam,
                onContentTypeSelected = onContentTypeSelected,
            )
        }
    }

    LaunchedEffect(selectedContentTypeFilter) {
        val selectedItemInfo =
            listState.layoutInfo.visibleItemsInfo.firstOrNull { it.key == selectedContentTypeFilter }

        if (selectedItemInfo == null || listState.isItemPartiallyVisible(selectedItemInfo)) {
            listState.animateScrollToItem(contentTypeFilters.indexOf(selectedContentTypeFilter))
        }
    }
}

@OptIn(ExperimentalMaterialApi::class)
@Composable
private fun DownloadChip(
    selected: Boolean,
    contentTypeFilter: FileItem.ContentTypeFilter,
    modifier: Modifier = Modifier,
    onContentTypeSelected: (FileItem.ContentTypeFilter) -> Unit,
) {
    FilterChip(
        selected = selected,
        onClick = { onContentTypeSelected(contentTypeFilter) },
        shape = RoundedCornerShape(16.dp),
        border = if (selected) {
            null
        } else {
            BorderStroke(1.dp, FirefoxTheme.colors.borderPrimary)
        },
        colors = ChipDefaults.filterChipColors(
            selectedBackgroundColor = FirefoxTheme.colors.layerAccentNonOpaque,
            selectedContentColor = FirefoxTheme.colors.textPrimary,
            backgroundColor = FirefoxTheme.colors.layer1,
            contentColor = FirefoxTheme.colors.textPrimary,
        ),
        modifier = modifier.height(36.dp),
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            if (selected) {
                // Custom content is used instead of using the `leadingIcon` parameter as the paddings
                // are different and spec requires a specific padding between the icon and text.
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_checkmark_24),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                    modifier = Modifier.size(16.dp),
                )
            }
            Text(
                text = stringResource(id = contentTypeFilter.stringRes),
                style = if (selected) {
                    FirefoxTheme.typography.headline8
                } else {
                    FirefoxTheme.typography.body2
                },
            )
        }
    }
}

@Composable
private fun EmptyState(modifier: Modifier = Modifier) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(
            text = stringResource(id = R.string.download_empty_message_2),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline7,
        )

        Spacer(modifier = Modifier.height(8.dp))

        Text(
            text = stringResource(id = R.string.download_empty_description),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )
    }
}

private class DownloadsScreenPreviewModelParameterProvider :
    PreviewParameterProvider<DownloadUIState> {
    override val values: Sequence<DownloadUIState>
        get() = sequenceOf(
            DownloadUIState.INITIAL,
            DownloadUIState(
                items = listOf(
                    FileItem(
                        id = "1",
                        fileName = "File 1",
                        url = "https://example.com/file1",
                        formattedSize = "1.2 MB",
                        displayedShortUrl = "example.com",
                        contentType = "application/pdf",
                        status = DownloadState.Status.COMPLETED,
                        filePath = "/path/to/file1",
                        createdTime = CreatedTime.TODAY,
                    ),
                    FileItem(
                        id = "2",
                        fileName = "File 2",
                        url = "https://example.com/file2",
                        formattedSize = "2.3 MB",
                        displayedShortUrl = "example.com",
                        contentType = "image/png",
                        status = DownloadState.Status.COMPLETED,
                        filePath = "/path/to/file1",
                        createdTime = CreatedTime.TODAY,
                    ),
                    FileItem(
                        id = "3",
                        fileName = "File 3",
                        url = "https://example.com/file3",
                        formattedSize = "3.4 MB",
                        displayedShortUrl = "example.com",
                        contentType = "application/zip",
                        status = DownloadState.Status.COMPLETED,
                        filePath = "/path/to/file1",
                        createdTime = CreatedTime.OLDER,
                    ),
                ),
                mode = DownloadUIState.Mode.Normal,
                pendingDeletionIds = emptySet(),
                userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
            ),
        )
}

@Composable
@FlexibleWindowLightDarkPreview
private fun DownloadsScreenPreviews(
    @PreviewParameter(DownloadsScreenPreviewModelParameterProvider::class) state: DownloadUIState,
) {
    val store = remember { DownloadUIStore(initialState = state) }
    val snackbarHostState = remember { AcornSnackbarHostState() }
    val scope = rememberCoroutineScope()
    FirefoxTheme {
        Box {
            DownloadsScreen(
                downloadsStore = store,
                onItemClick = {
                    scope.launch {
                        snackbarHostState.showSnackbar(
                            SnackbarState(message = "Item ${it.fileName} clicked"),
                        )
                    }
                },
                onItemDeleteClick = {
                    store.dispatch(DownloadUIAction.UpdateFileItems(store.state.items - it))
                    scope.launch {
                        snackbarHostState.showSnackbar(
                            SnackbarState(
                                message = "Item ${it.fileName} deleted",
                                type = SnackbarState.Type.Warning,
                            ),
                        )
                    }
                },
            )
            SnackbarHost(
                snackbarHostState = snackbarHostState,
                modifier = Modifier.align(Alignment.BottomCenter),
            )
        }
    }
}
