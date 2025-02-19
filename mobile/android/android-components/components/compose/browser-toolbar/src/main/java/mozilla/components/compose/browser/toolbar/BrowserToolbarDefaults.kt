/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.runtime.Composable
import mozilla.components.compose.base.theme.AcornTheme

/**
 * Contains the default values used by the browser toolbar.
 */
object BrowserToolbarDefaults {

    /**
     * Creates a [BrowserToolbarColors] that represents the default colors used in a browser
     * toolbar.
     */
    @Composable
    fun colors(
        customTabToolbarColors: CustomTabToolbarColors = CustomTabToolbarColors(
            background = AcornTheme.colors.layer1,
            title = AcornTheme.colors.textPrimary,
            url = AcornTheme.colors.textPrimary,
        ),
        displayToolbarColors: BrowserDisplayToolbarColors = BrowserDisplayToolbarColors(
            background = AcornTheme.colors.layer1,
            urlBackground = AcornTheme.colors.layer3,
            text = AcornTheme.colors.textPrimary,
        ),
        editToolbarColors: BrowserEditToolbarColors = BrowserEditToolbarColors(
            background = AcornTheme.colors.layer1,
            urlBackground = AcornTheme.colors.layer3,
            text = AcornTheme.colors.textPrimary,
            clearButton = AcornTheme.colors.iconPrimary,
        ),
    ) = BrowserToolbarColors(
        customTabToolbarColor = customTabToolbarColors,
        displayToolbarColors = displayToolbarColors,
        editToolbarColors = editToolbarColors,
    )
}
