/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import org.mozilla.fenix.compose.ext.isItemPartiallyVisible
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The row of filters displayed on the downloads screen.
 *
 * @param selectedContentTypeFilter The content type filter that is currently selected.
 * @param contentTypeFilters The content type filters to be displayed.
 * @param modifier Modifier to be applied to the row layout.
 * @param onContentTypeSelected Invoked when a filter is clicked.
 */
@Composable
internal fun Filters(
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
