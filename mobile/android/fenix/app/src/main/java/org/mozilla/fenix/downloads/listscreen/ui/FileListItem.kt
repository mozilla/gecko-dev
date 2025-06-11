/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.compose.base.menu.DropdownMenu
import mozilla.components.compose.base.menu.MenuItem
import mozilla.components.compose.base.text.Text
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.list.SelectableListItem
import org.mozilla.fenix.downloads.listscreen.DownloadsListTestTag
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.downloads.listscreen.store.TimeCategory
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * [SelectableListItem] used for displaying download items on the downloads screen.
 *
 * @param fileItem [FileItem] representing a download item.
 * @param isSelected The selected state of the item.
 * @param isMenuIconVisible Whether the menu icon is visible on the download item.
 * @param modifier Modifier to be applied to the [SelectableListItem].
 * @param onDeleteClick Invoked when delete is clicked.
 * @param onShareUrlClick Invoked when share URL is clicked.
 * @param onShareFileClick Invoked when share file is clicked.
 */
@Composable
internal fun FileListItem(
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

private data class FileListItemPreviewState(
    val fieItem: FileItem,
    val isSelected: Boolean,
    val isMenuIconVisible: Boolean,
)

private class FileListItemParameterProvider : PreviewParameterProvider<FileListItemPreviewState> {
    override val values: Sequence<FileListItemPreviewState>
        get() = sequenceOf(
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "1",
                    url = "https://www.mozilla.org",
                    fileName = "TestJPG.jpg",
                    filePath = "",
                    displayedShortUrl = "mozilla.org",
                    contentType = "image/jpg",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.IN_PROGRESS,
                    description = "3.4 MB • mozilla.org ",
                ),
                isSelected = false,
                isMenuIconVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "2",
                    url = "https://www.google.com",
                    fileName = "TestPDF.pdf",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/pdf",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "1.2 GB • example.com",
                ),
                isSelected = false,
                isMenuIconVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "3",
                    url = "https://www.google.com",
                    fileName = "TestVideo.mp4",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "video/mp4",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.LAST_30_DAYS,
                    description = "63 MB • example.com",
                ),
                isSelected = false,
                isMenuIconVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "4",
                    url = "https://www.google.com",
                    fileName = "TestZIP.zip",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/zip",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "30 MB • example.com",
                ),
                isSelected = false,
                isMenuIconVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "5",
                    url = "https://www.google.com",
                    fileName = "TestMSWordDoc.docx",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/msword",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "13 kB • example.com",
                ),
                isSelected = false,
                isMenuIconVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "6",
                    url = "https://www.mozilla.org",
                    fileName = "TestJPG.jpg",
                    filePath = "",
                    displayedShortUrl = "mozilla.org",
                    contentType = "image/jpg",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.OLDER,
                    description = "10 MB • example.com",
                ),
                isSelected = true,
                isMenuIconVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "7",
                    url = "https://www.google.com",
                    fileName = "TestPDF.pdf",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/pdf",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "20 MB • example.com",
                ),
                isSelected = true,
                isMenuIconVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "8",
                    url = "https://www.google.com",
                    fileName = "TestVideo.mp4",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "video/mp4",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "6 GB • example.com",
                ),
                isSelected = true,
                isMenuIconVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "4",
                    url = "https://www.google.com",
                    fileName = "TestZIP.zip",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/zip",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.TODAY,
                    description = "31 kB • example.com",
                ),
                isSelected = true,
                isMenuIconVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "5",
                    url = "https://www.google.com",
                    fileName = "TestMSWordDoc.docx",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/msword",
                    status = DownloadState.Status.COMPLETED,
                    timeCategory = TimeCategory.OLDER,
                    description = "66 MB • example.com",
                ),
                isSelected = true,
                isMenuIconVisible = false,
            ),
        )
}

@PreviewLightDark
@Composable
private fun FileListItemPreview(
    @PreviewParameter(FileListItemParameterProvider::class) fileListItemPreviewState: FileListItemPreviewState,
) {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(FirefoxTheme.colors.layer1),
        ) {
            FileListItem(
                isSelected = fileListItemPreviewState.isSelected,
                fileItem = fileListItemPreviewState.fieItem,
                isMenuIconVisible = fileListItemPreviewState.isMenuIconVisible,
                onShareFileClick = {},
                onDeleteClick = {},
                onShareUrlClick = {},
            )
            Spacer(modifier = Modifier.height(20.dp))
        }
    }
}
