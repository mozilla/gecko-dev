/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.theme.layout

import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.layout.AcornSize.MaxWidth.large
import mozilla.components.compose.base.theme.layout.AcornSize.MaxWidth.medium
import mozilla.components.compose.base.theme.layout.AcornSize.MaxWidth.small

private const val DEFAULT_FONT_SCALE = 1.0f

/**
 * A palette defining the static or dynamic sizing dimensions of visual elements styled by
 * the Acorn Design System.
 */
sealed class AcornSize {

    /**
     * A palette defining the static sizing dimensions of visual elements styled by
     * the Acorn Design System.
     */
    val static100: Dp = 8.dp
    val static200: Dp = 16.dp
    val static250: Dp = 20.dp
    val static300: Dp = 24.dp
    val static400: Dp = 32.dp
    val static600: Dp = 48.dp
    val static800: Dp = 64.dp
    val static1000: Dp = 80.dp
    val static1200: Dp = 96.dp
    val static1300: Dp = 104.dp
    val static1400: Dp = 112.dp

    /**
     * A palette defining the dynamic sizing dimensions of visual elements styled by
     * the Acorn Design System.
     */
    abstract val dynamic100: Dp
    abstract val dynamic200: Dp
    abstract val dynamic250: Dp
    abstract val dynamic300: Dp
    abstract val dynamic400: Dp
    abstract val dynamic600: Dp
    abstract val dynamic800: Dp
    abstract val dynamic1000: Dp
    abstract val dynamic1200: Dp
    abstract val dynamic1300: Dp
    abstract val dynamic1400: Dp

    /**
     * [AcornSize] tokens for [AcornWindowSize.Small].
     */
    data object Small : AcornSize() {
        override val dynamic100: Dp = 8.dp
        override val dynamic200: Dp = 16.dp
        override val dynamic250: Dp = 20.dp
        override val dynamic300: Dp = 24.dp
        override val dynamic400: Dp = 32.dp
        override val dynamic600: Dp = 48.dp
        override val dynamic800: Dp = 64.dp
        override val dynamic1000: Dp = 80.dp
        override val dynamic1200: Dp = 96.dp
        override val dynamic1300: Dp = 104.dp
        override val dynamic1400: Dp = 112.dp
    }

    /**
     * [AcornSize] tokens for [AcornWindowSize.Medium] and [AcornWindowSize.Large].
     */
    data object MediumLarge : AcornSize() {
        override val dynamic100: Dp = 16.dp
        override val dynamic200: Dp = 24.dp
        override val dynamic250: Dp = 24.dp
        override val dynamic300: Dp = 32.dp
        override val dynamic400: Dp = 48.dp
        override val dynamic600: Dp = 64.dp
        override val dynamic800: Dp = 80.dp
        override val dynamic1000: Dp = 96.dp
        override val dynamic1200: Dp = 104.dp
        override val dynamic1300: Dp = 112.dp
        override val dynamic1400: Dp = 120.dp
    }

    /**
     * The diameter for circular progress indicators.
     */
    val circularIndicatorDiameter: Dp
        get() = dynamic300

    /**
     * @see MaxWidth
     */
    val maxWidth: MaxWidth = MaxWidth

    /**
     * A palette of tokens defining the maximum widths certain visual elements can grow to.
     *
     * @property small Max-width for buttons, form elements, etc.
     * @property medium Default container max-width.
     * @property large Default container max-width when user has increased device font size.
     */
    object MaxWidth {
        val small: Dp = 400.dp
        val medium: Dp = 600.dp
        val large: Dp = 800.dp
    }

    /**
     * The max-width value for a container depending if font scaling is enabled.
     */
    val containerMaxWidth: Dp
        @Composable get() {
            return if (LocalDensity.current.fontScale > DEFAULT_FONT_SCALE) {
                large
            } else {
                medium
            }
        }

    /**
     * [AcornSize] helper object
     */
    companion object {
        /**
         * Returns the palette of size tokens corresponding to the [AcornWindowSize].
         *
         * @param windowSize The app window's current [AcornWindowSize].
         */
        fun fromWindowSize(windowSize: AcornWindowSize): AcornSize = when (windowSize) {
            AcornWindowSize.Small -> Small
            AcornWindowSize.Medium, AcornWindowSize.Large -> MediumLarge
        }
    }
}
