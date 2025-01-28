/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.theme.layout

import android.content.Context
import android.content.res.Configuration
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

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
        windowWidthMax = 600.dp,
    ),

    /**
     * Window sizes between 600 and 840.
     */
    Medium(
        windowWidthMax = 840.dp,
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

    /**
     * [AcornWindowSize] helper object
     */
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
