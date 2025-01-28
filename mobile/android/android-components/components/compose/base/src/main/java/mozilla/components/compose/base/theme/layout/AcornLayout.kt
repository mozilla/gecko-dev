/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.theme.layout

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowPreview
import mozilla.components.compose.base.theme.AcornTheme

/**
 * A palette of tokens defining the layout of visual elements styled by the Acorn Design System.
 */
data class AcornLayout(
    val space: AcornSpace,
    val size: AcornSize,
) {

    val border: AcornBorder = AcornBorder
    val corner: AcornCorner = AcornCorner
    val elevation: AcornElevation = AcornElevation

    /**
     * A palette of tokens defining the borders of visual elements styled by the Acorn Design System.
     */
    object AcornBorder {
        val thin: Dp = 1.dp
        val normal: Dp = 2.dp
        val thick: Dp = 4.dp
    }

    /**
     * A palette of tokens defining the corner radii of visual elements styled by the Acorn Design System.
     */
    object AcornCorner {
        val small: Dp = 2.dp
        val medium: Dp = 4.dp
        val large: Dp = 8.dp
        val xLarge: Dp = 16.dp
    }

    /**
     * A palette of tokens defining the elevation of visual elements styled by the Acorn Design System.
     */
    object AcornElevation {
        val xSmall: Dp = 1.dp
        val small: Dp = 2.dp
        val medium: Dp = 4.dp
        val large: Dp = 6.dp
        val xLarge: Dp = 8.dp
    }

    /**
     * [AcornLayout] helper object
     */
    companion object {
        /**
         * Returns the palette of layout tokens corresponding to the [AcornWindowSize].
         *
         * @param windowSize The app window's current [AcornWindowSize].
         */
        fun fromWindowSize(windowSize: AcornWindowSize) = AcornLayout(
            space = AcornSpace.fromWindowSize(windowSize = windowSize),
            size = AcornSize.fromWindowSize(windowSize = windowSize),
        )
    }
}

private const val GRID_ITEMS = 200

@OptIn(ExperimentalLayoutApi::class)
@FlexibleWindowPreview
@Composable
private fun AcornLayoutPreview() {
    AcornTheme {
        FlowRow(
            modifier = Modifier
                .background(color = AcornTheme.colors.layerScrim)
                .fillMaxSize()
                .verticalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(AcornTheme.layout.space.dynamic400),
            verticalArrangement = Arrangement.spacedBy(AcornTheme.layout.space.dynamic400),
        ) {
            repeat(GRID_ITEMS) {
                val color = Color(
                    red = it,
                    green = 0,
                    blue = it,
                )

                Box(
                    modifier = Modifier
                        .size(size = AcornTheme.layout.size.static800)
                        .background(
                            color = color,
                            shape = RoundedCornerShape(size = AcornTheme.layout.corner.small),
                        )
                        .border(
                            width = AcornTheme.layout.border.normal,
                            color = Color(
                                red = color.red * 0.8f,
                                green = color.green * 0.8f,
                                blue = color.blue * 0.8f,
                            ),
                            shape = RoundedCornerShape(size = AcornTheme.layout.corner.small),
                        ),
                )
            }
        }
    }
}
