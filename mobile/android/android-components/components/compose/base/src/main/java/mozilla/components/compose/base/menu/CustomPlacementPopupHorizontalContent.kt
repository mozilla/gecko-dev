/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.menu

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme

/**
 * [LazyRow] wrapper over [content] configuring it to be shown inside of [CustomPlacementPopup].
 *
 * @param content Composable items to be shown as horizontal content in a [CustomPlacementPopup].
 */
@Composable
inline fun CustomPlacementPopup.CustomPlacementPopupHorizontalContent(
    crossinline content: LazyListScope.() -> Unit,
) {
    LazyRow(
        modifier = Modifier
            .background(AcornTheme.colors.layer2)
            .height(48.dp),
    ) {
        content()
    }
}
