/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.ext

import androidx.compose.foundation.lazy.LazyListItemInfo
import androidx.compose.foundation.lazy.LazyListState

/**
 * Returns true if the item is partially visible in the list.
 *
 * @param itemInfo The [LazyListItemInfo] of the item to check.
 */
fun LazyListState.isItemPartiallyVisible(itemInfo: LazyListItemInfo): Boolean =
    (itemInfo.offset + itemInfo.size > layoutInfo.viewportEndOffset || itemInfo.offset < 0)
