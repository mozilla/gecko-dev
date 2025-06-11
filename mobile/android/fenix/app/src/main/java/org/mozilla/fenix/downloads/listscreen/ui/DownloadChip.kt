/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ChipDefaults
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.FilterChip
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Chip for displaying the different filters on the downloads screen.
 *
 * @param selected Whether the chip is selected.
 * @param contentTypeFilter The content type filter.
 * @param modifier Modifier to be applied to the chip.
 * @param onContentTypeSelected Invoked when the chip is clicked.
 */
@OptIn(ExperimentalMaterialApi::class)
@Composable
internal fun DownloadChip(
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

private data class DownloadChipPreviewState(
    val selected: Boolean,
    val contentTypeFilter: FileItem.ContentTypeFilter,
)

private class DownloadChipParameterProvider : PreviewParameterProvider<DownloadChipPreviewState> {
    override val values: Sequence<DownloadChipPreviewState>
        get() = sequenceOf(
            DownloadChipPreviewState(
                selected = true,
                contentTypeFilter = FileItem.ContentTypeFilter.Document,
            ),
            DownloadChipPreviewState(
                selected = false,
                contentTypeFilter = FileItem.ContentTypeFilter.Document,
            ),
            DownloadChipPreviewState(
                selected = true,
                contentTypeFilter = FileItem.ContentTypeFilter.Video,
            ),
            DownloadChipPreviewState(
                selected = false,
                contentTypeFilter = FileItem.ContentTypeFilter.Video,
            ),
        )
}

@PreviewLightDark
@Composable
private fun DownloadChipPreview(
    @PreviewParameter(DownloadChipParameterProvider::class) downloadChipPreviewState: DownloadChipPreviewState,
) {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(FirefoxTheme.colors.layer1),
        ) {
            DownloadChip(
                selected = downloadChipPreviewState.selected,
                contentTypeFilter = downloadChipPreviewState.contentTypeFilter,
                onContentTypeSelected = {},
            )
        }
    }
}
