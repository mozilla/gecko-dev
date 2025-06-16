/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package mozilla.components.compose.base.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.remember
import androidx.compose.runtime.staticCompositionLocalOf
import mozilla.components.compose.base.theme.layout.AcornLayout
import mozilla.components.compose.base.theme.layout.AcornWindowSize

/**
 * A top-level Composable wrapper used to access Acorn Theming tokens.
 *
 * @param colors The [AcornColors] theme to use.
 * @param content The children composables to be laid out.
 */
@Composable
fun AcornTheme(
    colors: AcornColors = getAcornColors(),
    colorScheme: ColorScheme = getAcornColorScheme(),
    content: @Composable () -> Unit,
) {
    ProvideAcornTokens(
        colors = colors,
    ) {
        MaterialTheme(
            colorScheme = colorScheme,
            content = content,
        )
    }
}

@Composable
private fun getAcornColors() = if (isSystemInDarkTheme()) {
    darkColorPalette
} else {
    lightColorPalette
}

@Composable
private fun getAcornColorScheme(): ColorScheme = if (isSystemInDarkTheme()) {
    acornDarkColorScheme()
} else {
    acornLightColorScheme()
}

/**
 * Provides access to the Acorn design system tokens.
 */
object AcornTheme {
    val colors: AcornColors
        @Composable
        get() = localAcornColors.current

    val typography: AcornTypography
        get() = defaultTypography

    val layout: AcornLayout
        @Composable
        get() = localLayout.current

    val windowSize: AcornWindowSize
        @Composable
        get() = localWindowSize.current
}

/**
 * This function is used to set the current value of [localWindowSize],
 * [localLayout], and [localAcornColors].
 */
@Composable
private fun ProvideAcornTokens(
    windowSize: AcornWindowSize = AcornWindowSize.getWindowSize(),
    colors: AcornColors,
    content: @Composable () -> Unit,
) {
    val layout = remember(windowSize) {
        AcornLayout.fromWindowSize(windowSize = windowSize)
    }
    val colorPalette = remember {
        // Explicitly creating a new object here so we don't mutate the initial [colors]
        // provided, and overwrite the values set in it.
        colors.copy()
    }
    colorPalette.update(colors)

    CompositionLocalProvider(
        localWindowSize provides windowSize,
        localLayout provides layout,
        localAcornColors provides colorPalette,
        content = content,
    )
}

private val localAcornColors = staticCompositionLocalOf<AcornColors> {
    error("No AcornColors provided")
}

private val localWindowSize = staticCompositionLocalOf<AcornWindowSize> {
    error("No AcornWindowSize provided")
}

private val localLayout = staticCompositionLocalOf {
    AcornLayout.fromWindowSize(windowSize = AcornWindowSize.Small)
}
