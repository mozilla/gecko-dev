/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.ui.icons.R

@Composable
internal fun DotHighlight(
    modifier: Modifier = Modifier,
    color: Color = AcornTheme.colors.iconNotice,
) {
    Canvas(modifier = modifier.size(10.dp)) {
        val canvasWidth = size.width
        val canvasHeight = size.height

        drawCircle(
            color = color,
            center = Offset(x = canvasWidth / 2, y = canvasHeight / 2),
            radius = canvasWidth / 2,
        )
    }
}

@PreviewLightDark
@Composable
private fun HighlightedActionButtonPreview() {
    AcornTheme {
        Box(modifier = Modifier.background(AcornTheme.colors.layer1)) {
            Image(
                painter = painterResource(id = R.drawable.mozac_ic_web_extension_default_icon),
                contentDescription = null,
                modifier = Modifier.size(32.dp),
                colorFilter = ColorFilter.tint(AcornTheme.colors.iconPrimary),
            )
            DotHighlight(
                modifier = Modifier.align(Alignment.BottomEnd),
            )
        }
    }
}
