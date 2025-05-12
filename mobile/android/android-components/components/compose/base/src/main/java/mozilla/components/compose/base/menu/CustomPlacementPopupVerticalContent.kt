/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.menu

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.requiredWidthIn
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme

/**
 * [Column] wrapper over [content] configuring it to be shown inside of [CustomPlacementPopup].
 *
 * @param content Composable content to be shown as vertical content in a [CustomPlacementPopup].
 */
@Composable
inline fun CustomPlacementPopup.CustomPlacementPopupVerticalContent(
    content: @Composable ColumnScope.() -> Unit,
) {
    Column(
        modifier = Modifier
            .background(AcornTheme.colors.layer2)
            .requiredWidthIn(min = 250.dp)
            .width(IntrinsicSize.Max)
            .verticalScroll(rememberScrollState()),
    ) {
        content()
    }
}
