/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.theme.layout

import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

/**
 * A palette defining the static and dynamic whitespace and arrangement between visual elements
 * styled by the Acorn Design System.
 */
sealed class AcornSpace {

    /**
     * A palette defining the static whitespace and arrangement between visual elements
     * styled by the Acorn Design System.
     */
    val static50: Dp = 4.dp
    val static100: Dp = 8.dp
    val static150: Dp = 12.dp
    val static200: Dp = 16.dp
    val static300: Dp = 24.dp
    val static400: Dp = 32.dp
    val static500: Dp = 40.dp
    val static600: Dp = 48.dp

    /**
     * A palette defining the dynamic whitespace and arrangement between visual elements
     * styled by the Acorn Design System.
     */
    abstract val dynamic50: Dp
    abstract val dynamic100: Dp
    abstract val dynamic150: Dp
    abstract val dynamic200: Dp
    abstract val dynamic300: Dp
    abstract val dynamic400: Dp
    abstract val dynamic500: Dp
    abstract val dynamic600: Dp

    /**
     * [AcornSpace] tokens for [AcornWindowSize.Small].
     */
    data object Small : AcornSpace() {
        override val dynamic50: Dp = 4.dp
        override val dynamic100: Dp = 8.dp
        override val dynamic150: Dp = 12.dp
        override val dynamic200: Dp = 16.dp
        override val dynamic300: Dp = 24.dp
        override val dynamic400: Dp = 32.dp
        override val dynamic500: Dp = 40.dp
        override val dynamic600: Dp = 48.dp
    }

    /**
     * [AcornSpace] tokens for [AcornWindowSize.Medium] and [AcornWindowSize.Large].
     */
    data object MediumLarge : AcornSpace() {
        override val dynamic50: Dp = 8.dp
        override val dynamic100: Dp = 12.dp
        override val dynamic150: Dp = 16.dp
        override val dynamic200: Dp = 24.dp
        override val dynamic300: Dp = 32.dp
        override val dynamic400: Dp = 40.dp
        override val dynamic500: Dp = 48.dp
        override val dynamic600: Dp = 56.dp
    }

    /**
     * [AcornSpace] helper object
     */
    companion object {
        /**
         * Returns the palette of space tokens corresponding to the [AcornWindowSize].
         *
         * @param windowSize The app window's current [AcornWindowSize].
         */
        fun fromWindowSize(windowSize: AcornWindowSize): AcornSpace = when (windowSize) {
            AcornWindowSize.Small -> Small
            AcornWindowSize.Medium, AcornWindowSize.Large -> MediumLarge
        }
    }
}
