/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.theme

import android.content.Context
import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.utils.isLargeScreenSize

/**
 * The baseline dimension primitives for size, space, and window size.
 */
private object LayoutPrimitives {
    // Size
    val sizeXXSmall: Dp = 16.dp
    val sizeXSmall: Dp = 24.dp
    val sizeSmall: Dp = 32.dp
    val sizeMedium: Dp = 48.dp
    val sizeLarge: Dp = 64.dp
    val sizeXLarge: Dp = 80.dp
    val sizeXXLarge: Dp = 96.dp

    // Corner radius
    val cornerRadiusSmall: Dp = 2.dp
    val cornersRadiusMedium: Dp = 4.dp
    val cornerRadiusLarge: Dp = 8.dp
    val cornerRadiusXLarge: Dp = 16.dp

    // Border
    val borderThin: Dp = 1.dp
    val borderNormal: Dp = 2.dp
    val borderThick: Dp = 4.dp

    // Elevation
    val elevationXSmall: Dp = 1.dp
    val elevationSmall: Dp = 2.dp
    val elevationMedium: Dp = 4.dp
    val elevationLarge: Dp = 6.dp
    val elevationXLarge: Dp = 8.dp

    // Space
    // The baseline values used for Small, Medium, and Large window sizes.
    val spaceSmall: Dp = 8.dp
    val spaceMedium: Dp = 16.dp
    val spaceLarge: Dp = 32.dp

    // Window size breakpoints
    val windowBreakpointSmallMax: Dp = 600.dp
    val windowBreakpointMediumMax: Dp = 840.dp
}

/**
 * A custom size palette defining the corner radii of visual elements styled by the Acorn Design System.
 *
 * @property small The kdocs will be written as part of https://bugzilla.mozilla.org/show_bug.cgi?id=1908017
 * @property medium Placeholder for detekt.
 * @property large Placeholder for detekt.
 * @property xLarge Placeholder for detekt.
*/
data class AcornCorner(
    val small: Dp = LayoutPrimitives.cornerRadiusSmall,
    val medium: Dp = LayoutPrimitives.cornersRadiusMedium,
    val large: Dp = LayoutPrimitives.cornerRadiusLarge,
    val xLarge: Dp = LayoutPrimitives.cornerRadiusXLarge,
)

/**
 * A custom size palette defining the borders of visual elements styled by the Acorn Design System.
 *
 * @property thin Placeholder for detekt.
 * @property normal Placeholder for detekt.
 * @property thick The kdocs will be written as part of https://bugzilla.mozilla.org/show_bug.cgi?id=1908017
 */
data class AcornBorder(
    val thin: Dp = LayoutPrimitives.borderThin,
    val normal: Dp = LayoutPrimitives.borderNormal,
    val thick: Dp = LayoutPrimitives.borderThick,
)

/**
 * A custom size palette defining the elevation of visual elements styled by the Acorn Design System.
 *
 * @property xSmall The kdocs will be written as part of https://bugzilla.mozilla.org/show_bug.cgi?id=1908017
 * @property small Placeholder for detekt.
 * @property medium Placeholder for detekt.
 * @property large Placeholder for detekt.
 * @property xLarge Placeholder for detekt.
 */
data class AcornElevation(
    val xSmall: Dp = LayoutPrimitives.elevationXSmall,
    val small: Dp = LayoutPrimitives.elevationSmall,
    val medium: Dp = LayoutPrimitives.elevationMedium,
    val large: Dp = LayoutPrimitives.elevationLarge,
    val xLarge: Dp = LayoutPrimitives.elevationXLarge,
)

/**
 * A custom size palette defining the static dimensions of visual elements styled by the Acorn Design System.
 *
 * @property xxSmall The kdocs will be written as part of https://bugzilla.mozilla.org/show_bug.cgi?id=1908017
 * @property xSmall Placeholder for detekt.
 * @property small Placeholder for detekt.
 * @property medium Placeholder for detekt.
 * @property large Placeholder for detekt.
 * @property xLarge Placeholder for detekt.
 * @property xxLarge Placeholder for detekt.
 * @property border Placeholder for detekt.
 * @property corner Placeholder for detekt.
 * @property elevation Placeholder for detekt.
 */
data class AcornSize(
    val xxSmall: Dp = LayoutPrimitives.sizeXXSmall,
    val xSmall: Dp = LayoutPrimitives.sizeXSmall,
    val small: Dp = LayoutPrimitives.sizeSmall,
    val medium: Dp = LayoutPrimitives.sizeMedium,
    val large: Dp = LayoutPrimitives.sizeLarge,
    val xLarge: Dp = LayoutPrimitives.sizeXLarge,
    val xxLarge: Dp = LayoutPrimitives.sizeXXLarge,
    val border: AcornBorder = AcornBorder(),
    val corner: AcornCorner = AcornCorner(),
    val elevation: AcornElevation = AcornElevation(),
) {
    // Alias tokens
    /**
     * Placeholder for detekt.
     */
    val favicon = medium

    /**
     * Placeholder for detekt.
     */
    val thumbnail = large

    /**
     * Placeholder for detekt.
     */
    val circularIndicatorDiameter = xSmall

    /**
     * The maximum width that a container can take when it is the sole UI element on the screen.
     * This will adapt a phone-centric design into a large screen viewport.
     */
    val containerMaxWidth = LayoutPrimitives.windowBreakpointSmallMax
}

/**
 * A custom space palette defining the whitespace and arrangement between visual elements
 * styled by the Acorn Design System.
 *
 * @param baselineSpaceValue The baseline value from which sizes XXS and S-XXL are derived from.
 */
data class AcornSpace(
    private val baselineSpaceValue: Dp,
) {
    // General use tokens
    /**
     * Placeholder for detekt.
     */
    val xxSmall: Dp = baselineSpaceValue * 0.5f

    /**
     * Placeholder for detekt.
     */
    val xSmall: Dp = baselineSpaceValue

    /**
     * Placeholder for detekt.
     */
    val small: Dp = baselineSpaceValue * 2

    /**
     * Placeholder for detekt.
     */
    val medium: Dp = baselineSpaceValue * 3

    /**
     * Placeholder for detekt.
     */
    val large: Dp = baselineSpaceValue * 4

    /**
     * Placeholder for detekt.
     */
    val xLarge: Dp = baselineSpaceValue * 6

    /**
     * Placeholder for detekt.
     */
    val xxLarge: Dp = baselineSpaceValue * 8

    // Alias tokens
    /**
     * Placeholder for detekt.
     */
    val cardPadding = small

    // Alias tokens for padding within a container layout
    /**
     * Placeholder for detekt.
     */
    val baseContentHorizontalPadding = small

    /**
     * Placeholder for detekt.
     */
    val baseContentVerticalPadding = small

    /**
     * Placeholder for detekt.
     */
    val baseContentEqualPadding = small

    companion object {
        /**
         * Returns the palette of space tokens corresponding to the [AcornWindowSize].
         *
         * @param windowSize The app window's current [AcornWindowSize].
         */
        fun fromWindowSize(windowSize: AcornWindowSize): AcornSpace = when (windowSize) {
            AcornWindowSize.Small -> AcornSpace(baselineSpaceValue = LayoutPrimitives.spaceSmall)
            AcornWindowSize.Medium -> AcornSpace(baselineSpaceValue = LayoutPrimitives.spaceMedium)
            AcornWindowSize.Large -> AcornSpace(baselineSpaceValue = LayoutPrimitives.spaceLarge)
        }
    }
}

/**
 * The window size buckets for deriving the layout of visual elements styled by the Acorn Design System.
 *
 * @param windowWidthMax The maximum width value the window can fall into.
 */
enum class AcornWindowSize(
    private val windowWidthMax: Dp,
) {
    /**
     * Window sizes between 0 and 600.
     */
    Small(
        windowWidthMax = LayoutPrimitives.windowBreakpointSmallMax,
    ),

    /**
     * Window sizes between 600 and 840.
     */
    Medium(
        windowWidthMax = LayoutPrimitives.windowBreakpointMediumMax,
    ),

    /**
     * Window sizes 840 or larger.
     */
    Large(
        windowWidthMax = Dp.Infinity,
    ),
    ;

    /**
     * Helper function to determine when to make UI differences for layouts in
     * window sizes other than [AcornWindowSize.Small].
     *
     * @return Whether this instance of [AcornWindowSize] is not [Small].
     */
    fun isNotSmall() = this != Small

    companion object {
        /**
         * Helper function for obtaining the window's [AcornWindowSize] within Compose.
         *
         * @return The [AcornWindowSize] that corresponds to the current window width.
         */
        @Composable
        fun getWindowSize(): AcornWindowSize = getWindowSizeToken(
            configuration = LocalConfiguration.current,
            measureUsingAllScreenEdges = false,
        )

        /**
         * Helper function for obtaining the window's [AcornWindowSize] via a [Context].
         *
         * @param context [Context] used to obtain the current [Configuration].
         * @return The [AcornWindowSize] that corresponds to the current window width.
         */
        fun getWindowSize(context: Context): AcornWindowSize = getWindowSizeToken(
            configuration = context.resources.configuration,
            measureUsingAllScreenEdges = false,
        )

        /**
         * Helper function used to determine when the app's total *window* size is at least the
         * size of a tablet. To determine whether the device's *physical* size is at least
         * the size of a tablet, use [Context.isLargeScreenSize] instead.
         *
         * @return true if the app has a window size that is not [AcornWindowSize.Small].
         */
        @Composable
        fun isLargeWindow(): Boolean = getWindowSizeToken(
            configuration = LocalConfiguration.current,
            measureUsingAllScreenEdges = true,
        ).isNotSmall()

        /**
         * Helper function used to determine when the app's total *window* size is at least the
         * size of a tablet. To determine whether the device's *physical* size is at least
         * the size of a tablet, use [Context.isLargeScreenSize] instead.
         *
         * @param context [Context] used to obtain the current [Configuration].
         * @return true if the app has a window size that is not [AcornWindowSize.Small].
         */
        fun isLargeWindow(context: Context): Boolean = getWindowSizeToken(
            configuration = context.resources.configuration,
            measureUsingAllScreenEdges = true,
        ).isNotSmall()

        /**
         * Internal function for deriving the window's [AcornWindowSize].
         *
         * @param configuration [Configuration] used to obtain the window's screen width.
         * @param measureUsingAllScreenEdges Whether to derive the [AcornWindowSize] by considering all screen edges.
         * @return The [AcornWindowSize] calculated from the window's screen width. See [AcornWindowSize]
         *  for possible values.
         */
        private fun getWindowSizeToken(
            configuration: Configuration,
            measureUsingAllScreenEdges: Boolean,
        ): AcornWindowSize {
            val width = if (measureUsingAllScreenEdges) {
                configuration.smallestScreenWidthDp.dp
            } else {
                configuration.screenWidthDp.dp
            }

            return when {
                width < Small.windowWidthMax -> Small
                width < Medium.windowWidthMax -> Medium
                else -> Large
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Preview(widthDp = 1000)
@Suppress("LongMethod", "MagicNumber")
@Composable
private fun AcornLayoutPreview() {
    var windowSize by remember { mutableStateOf(AcornWindowSize.Large) }

    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layerScrim)
                .fillMaxSize()
                .padding(all = FirefoxTheme.space.cardPadding),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(FirefoxTheme.space.small),
            ) {
                Button(
                    onClick = {
                        windowSize = AcornWindowSize.Small
                    },
                    modifier = Modifier.width(200.dp),
                    colors = ButtonDefaults.outlinedButtonColors(
                        backgroundColor = FirefoxTheme.colors.actionPrimary,
                    ),
                ) {
                    Text(
                        text = "Small",
                        color = FirefoxTheme.colors.textActionPrimary,
                        style = FirefoxTheme.typography.button,
                    )
                }

                Button(
                    onClick = {
                        windowSize = AcornWindowSize.Medium
                    },
                    modifier = Modifier.width(200.dp),
                    colors = ButtonDefaults.outlinedButtonColors(
                        backgroundColor = FirefoxTheme.colors.actionPrimary,
                    ),
                ) {
                    Text(
                        text = "Medium",
                        color = FirefoxTheme.colors.textActionPrimary,
                        style = FirefoxTheme.typography.button,
                    )
                }

                Button(
                    onClick = {
                        windowSize = AcornWindowSize.Large
                    },
                    modifier = Modifier.width(200.dp),
                    colors = ButtonDefaults.outlinedButtonColors(
                        backgroundColor = FirefoxTheme.colors.actionPrimary,
                    ),
                ) {
                    Text(
                        text = "Large",
                        color = FirefoxTheme.colors.textActionPrimary,
                        style = FirefoxTheme.typography.button,
                    )
                }
            }

            Spacer(Modifier.height(FirefoxTheme.space.small))

            FirefoxTheme(windowSize = windowSize) {
                val widthModifier = when (FirefoxTheme.windowSize) {
                    AcornWindowSize.Small -> Modifier.width(width = 400.dp)
                    AcornWindowSize.Medium -> Modifier.width(width = 700.dp)
                    AcornWindowSize.Large -> Modifier.fillMaxWidth()
                }
                FlowRow(
                    modifier = widthModifier
                        .fillMaxHeight()
                        .verticalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(FirefoxTheme.space.small),
                    verticalArrangement = Arrangement.spacedBy(FirefoxTheme.space.small),
                ) {
                    for (x in 0 until 200) {
                        val color = Color(
                            red = x,
                            green = 0,
                            blue = x,
                        )

                        Box(
                            modifier = Modifier
                                .size(size = FirefoxTheme.size.thumbnail)
                                .background(
                                    color = color,
                                    shape = RoundedCornerShape(size = FirefoxTheme.size.corner.small),
                                )
                                .border(
                                    width = FirefoxTheme.size.border.normal,
                                    color = color.copy(
                                        red = color.red * 0.8f,
                                        green = color.green * 0.8f,
                                        blue = color.blue * 0.8f,
                                    ),
                                    shape = RoundedCornerShape(size = FirefoxTheme.size.corner.small),
                                ),
                        )
                    }
                }
            }
        }
    }
}
