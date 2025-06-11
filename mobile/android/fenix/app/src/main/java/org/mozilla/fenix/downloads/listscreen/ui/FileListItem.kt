/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.ui

import androidx.annotation.FloatRange
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.LinearProgressIndicator
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
 * @param areAfterListItemIconsVisible Whether the menu icon is visible on the download item.
 * @param modifier Modifier to be applied to the [SelectableListItem].
 * @param onPauseClick Invoked when pause is clicked.
 * @param onResumeClick Invoked when resume is clicked.
 * @param onRetryClick Invoked when retry is clicked.
 * @param onDeleteClick Invoked when delete is clicked.
 * @param onShareUrlClick Invoked when share URL is clicked.
 * @param onShareFileClick Invoked when share file is clicked.
 */
@Composable
 @Suppress("LongParameterList")
internal fun FileListItem(
    fileItem: FileItem,
    isSelected: Boolean,
    areAfterListItemIconsVisible: Boolean,
    modifier: Modifier = Modifier,
    onPauseClick: (id: String) -> Unit,
    onResumeClick: (id: String) -> Unit,
    onRetryClick: (id: String) -> Unit,
    onDeleteClick: (FileItem) -> Unit,
    onShareUrlClick: (FileItem) -> Unit,
    onShareFileClick: (FileItem) -> Unit,
) {
    SelectableListItem(
        label = fileItem.fileName ?: fileItem.url,
        description = fileItem.description,
        icon = if (fileItem.status == FileItem.Status.Failed) R.drawable.mozac_ic_critical_24 else fileItem.icon,
        isSelected = isSelected,
        modifier = modifier,
        descriptionTextColor = if (fileItem.status == FileItem.Status.Failed) {
            FirefoxTheme.colors.iconCritical
        } else {
            FirefoxTheme.colors.textSecondary
        },
        iconTint = if (fileItem.status == FileItem.Status.Failed) {
            FirefoxTheme.colors.iconCritical
        } else {
            FirefoxTheme.colors.iconPrimary
        },
        afterListItemAction = {
            if (areAfterListItemIconsVisible) {
                AfterListItemAction(
                    fileItem = fileItem,
                    onPauseClick = onPauseClick,
                    onResumeClick = onResumeClick,
                    onRetryClick = onRetryClick,
                    onDeleteClick = onDeleteClick,
                    onShareUrlClick = onShareUrlClick,
                    onShareFileClick = onShareFileClick,
                )
            }
        },
        belowListItemContent = {
            when (fileItem.status) {
                FileItem.Status.Cancelled -> {}
                FileItem.Status.Completed -> {}
                is FileItem.Status.Downloading -> {
                    DownloadProgressIndicator(progress = fileItem.status.progress)
                }
                FileItem.Status.Failed -> {}
                FileItem.Status.Initiated -> {
                    DownloadProgressIndicator(progress = null)
                }
                is FileItem.Status.Paused -> {
                    DownloadProgressIndicator(progress = fileItem.status.progress)
                }
            }
        },
    )
}

@Composable
private fun AfterListItemAction(
    fileItem: FileItem,
    onPauseClick: (id: String) -> Unit,
    onResumeClick: (id: String) -> Unit,
    onRetryClick: (id: String) -> Unit,
    onDeleteClick: (FileItem) -> Unit,
    onShareUrlClick: (FileItem) -> Unit,
    onShareFileClick: (FileItem) -> Unit,
) {
    var menuExpanded by remember { mutableStateOf(false) }

    when (fileItem.status) {
        FileItem.Status.Completed -> {}
        FileItem.Status.Initiated -> {}
        is FileItem.Status.Downloading -> {
            IconButton(
                onClick = { onPauseClick(fileItem.id) },
            ) {
                Icon(
                    painter = painterResource(R.drawable.mozac_feature_media_action_pause),
                    contentDescription = stringResource(R.string.download_pause_action),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }
        is FileItem.Status.Paused -> {
            IconButton(
                onClick = { onResumeClick(fileItem.id) },
            ) {
                Icon(
                    painter = painterResource(R.drawable.mozac_feature_media_action_play),
                    contentDescription = stringResource(R.string.download_resume_action),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }
        FileItem.Status.Cancelled -> {}
        FileItem.Status.Failed -> {
            IconButton(
                onClick = { onRetryClick(fileItem.id) },
            ) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_arrow_counter_clockwise_24),
                    contentDescription = stringResource(R.string.download_retry_action),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }
    }

    Spacer(modifier = Modifier.width(24.dp))

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
            menuItems = getContextMenuItems(
                status = fileItem.status,
                onDeleteClick = { onDeleteClick(fileItem) },
                onShareUrlClick = { onShareUrlClick(fileItem) },
                onShareFileClick = { onShareFileClick(fileItem) },
            ),
            expanded = menuExpanded,
            onDismissRequest = { menuExpanded = false },
        )
    }
}

@Composable
private fun DownloadProgressIndicator(
    @FloatRange(from = 0.0, to = 1.0) progress: Float?,
) {
    Column {
        Spacer(modifier = Modifier.height(6.dp))

        if (progress == null) {
            LinearProgressIndicator(
                color = FirefoxTheme.colors.borderAccent,
                backgroundColor = FirefoxTheme.colors.borderPrimary,
            )
        } else {
            LinearProgressIndicator(
                progress = progress,
                color = FirefoxTheme.colors.borderAccent,
                backgroundColor = FirefoxTheme.colors.borderPrimary,
            )
        }
    }
}

private fun getContextMenuItems(
    status: FileItem.Status,
    onDeleteClick: () -> Unit,
    onShareUrlClick: () -> Unit,
    onShareFileClick: () -> Unit,
) = when (status) {
    FileItem.Status.Completed -> listOf(
        MenuItem.TextItem(
            text = Text.Resource(R.string.download_share_url),
            onClick = onShareUrlClick,
            level = MenuItem.FixedItem.Level.Default,
        ),
        MenuItem.TextItem(
            text = Text.Resource(R.string.download_share_file),
            onClick = onShareFileClick,
            level = MenuItem.FixedItem.Level.Default,
        ),
        MenuItem.TextItem(
            text = Text.Resource(R.string.download_delete_item),
            onClick = onDeleteClick,
            level = MenuItem.FixedItem.Level.Critical,
        ),
    )
    else -> listOf(
        MenuItem.TextItem(
            text = Text.Resource(R.string.download_share_url),
            onClick = onShareUrlClick,
            level = MenuItem.FixedItem.Level.Default,
        ),
        MenuItem.TextItem(
            text = Text.Resource(R.string.download_delete_item),
            onClick = onDeleteClick,
            level = MenuItem.FixedItem.Level.Critical,
        ),
    )
}

private data class FileListItemPreviewState(
    val fieItem: FileItem,
    val isSelected: Boolean,
    val areAfterListItemIconsVisible: Boolean,
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
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.IN_PROGRESS,
                    description = "3.4 MB • mozilla.org ",
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "2",
                    url = "https://www.google.com",
                    fileName = "TestPDF.pdf",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/pdf",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "1.2 GB • example.com",
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "3",
                    url = "https://www.google.com",
                    fileName = "TestVideo.mp4",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "video/mp4",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.LAST_30_DAYS,
                    description = "63 MB • example.com",
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "4",
                    url = "https://www.google.com",
                    fileName = "TestZIP.zip",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/zip",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "30 MB • example.com",
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "5",
                    url = "https://www.google.com",
                    fileName = "TestMSWordDoc.docx",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/msword",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "13 kB • example.com",
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "6",
                    url = "https://www.mozilla.org",
                    fileName = "TestJPG.jpg",
                    filePath = "",
                    displayedShortUrl = "mozilla.org",
                    contentType = "image/jpg",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.OLDER,
                    description = "10 MB • example.com",
                ),
                isSelected = true,
                areAfterListItemIconsVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "7",
                    url = "https://www.google.com",
                    fileName = "TestPDF.pdf",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/pdf",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "20 MB • example.com",
                ),
                isSelected = true,
                areAfterListItemIconsVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "8",
                    url = "https://www.google.com",
                    fileName = "TestVideo.mp4",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "video/mp4",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.YESTERDAY,
                    description = "6 GB • example.com",
                ),
                isSelected = true,
                areAfterListItemIconsVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "9",
                    url = "https://www.google.com",
                    fileName = "TestZIP.zip",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/zip",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.TODAY,
                    description = "31 kB • example.com",
                ),
                isSelected = true,
                areAfterListItemIconsVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "10",
                    url = "https://www.google.com",
                    fileName = "TestMSWordDoc.docx",
                    filePath = "",
                    displayedShortUrl = "google.com",
                    contentType = "application/msword",
                    status = FileItem.Status.Completed,
                    timeCategory = TimeCategory.OLDER,
                    description = "66 MB • example.com",
                ),
                isSelected = true,
                areAfterListItemIconsVisible = false,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "11",
                    fileName = "File 11",
                    url = "https://example.com/file11",
                    description = "5 MB / 10 MB • in 5s",
                    displayedShortUrl = "example.com",
                    contentType = "application/zip",
                    status = FileItem.Status.Downloading(progress = 0.5f),
                    filePath = "",
                    timeCategory = TimeCategory.IN_PROGRESS,
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "12",
                    fileName = "File 12",
                    url = "https://example.com/file12",
                    description = "5 MB / 10 MB • pending",
                    displayedShortUrl = "example.com",
                    contentType = "application/zip",
                    status = FileItem.Status.Downloading(progress = 0.5f),
                    filePath = "",
                    timeCategory = TimeCategory.IN_PROGRESS,
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "13",
                    fileName = "File 13",
                    url = "https://example.com/file13",
                    description = "5 MB / 10 MB • paused",
                    displayedShortUrl = "example.com",
                    contentType = "application/zip",
                    status = FileItem.Status.Paused(progress = 0.5f),
                    filePath = "",
                    timeCategory = TimeCategory.IN_PROGRESS,
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "14",
                    fileName = "File 14",
                    url = "https://example.com/file14",
                    description = "Preparing download…",
                    displayedShortUrl = "example.com",
                    contentType = "application/zip",
                    status = FileItem.Status.Initiated,
                    filePath = "",
                    timeCategory = TimeCategory.IN_PROGRESS,
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
            ),
            FileListItemPreviewState(
                fieItem = FileItem(
                    id = "15",
                    fileName = "File 15",
                    url = "https://example.com/file15",
                    description = "Download Failed",
                    displayedShortUrl = "example.com",
                    contentType = "application/zip",
                    status = FileItem.Status.Failed,
                    filePath = "",
                    timeCategory = TimeCategory.IN_PROGRESS,
                ),
                isSelected = false,
                areAfterListItemIconsVisible = true,
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
                areAfterListItemIconsVisible = fileListItemPreviewState.areAfterListItemIconsVisible,
                onPauseClick = {},
                onResumeClick = {},
                onRetryClick = {},
                onShareFileClick = {},
                onDeleteClick = {},
                onShareUrlClick = {},
            )
            Spacer(modifier = Modifier.height(20.dp))
        }
    }
}
