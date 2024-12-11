/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.ext.settings

/**
 * The theme for Mozilla Firefox for Android (Fenix).
 *
 * @param theme The current [Theme] that is displayed.
 * @param content The children composables to be laid out.
 */
@Composable
fun FirefoxTheme(
    theme: Theme = Theme.getTheme(),
    content: @Composable () -> Unit,
) {
    val colors = when (theme) {
        Theme.Light -> lightColorPalette
        Theme.Dark -> darkColorPalette
        Theme.Private -> privateColorPalette
    }

    AcornTheme(
        colors = colors,
        content = content,
    )
}

/**
 * Indicates the theme that is displayed.
 */
enum class Theme {
    Light,
    Dark,
    Private,
    ;

    companion object {
        /**
         * Returns the current [Theme] that is displayed.
         *
         * @param allowPrivateTheme Boolean used to control whether [Theme.Private] is an option
         * for [FirefoxTheme] colors.
         * @return the current [Theme] that is displayed.
         */
        @Composable
        fun getTheme(allowPrivateTheme: Boolean = true) =
            if (allowPrivateTheme &&
                !inComposePreview &&
                LocalContext.current.settings().lastKnownMode.isPrivate
            ) {
                Private
            } else if (isSystemInDarkTheme()) {
                Dark
            } else {
                Light
            }
    }
}

/**
 * Provides access to the Firefox design system tokens.
 */
object FirefoxTheme {
    val colors: AcornColors
        @Composable
        get() = AcornTheme.colors

    val typography: AcornTypography
        get() = AcornTheme.typography

    val size: AcornSize
        @Composable
        get() = AcornTheme.size

    val space: AcornSpace
        @Composable
        get() = AcornTheme.space

    val windowSize: AcornWindowSize
        @Composable
        get() = AcornTheme.windowSize
}
