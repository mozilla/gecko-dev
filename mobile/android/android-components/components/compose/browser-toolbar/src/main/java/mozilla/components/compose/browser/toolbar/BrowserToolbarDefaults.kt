/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable

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
        displayToolbarColors: BrowserDisplayToolbarColors = BrowserDisplayToolbarColors(
            background = MaterialTheme.colors.background,
            text = MaterialTheme.colors.onBackground,
        ),
        editToolbarColors: BrowserEditToolbarColors = BrowserEditToolbarColors(
            background = MaterialTheme.colors.background,
            text = MaterialTheme.colors.onBackground,
        ),
    ) = BrowserToolbarColors(
        displayToolbarColors = displayToolbarColors,
        editToolbarColors = editToolbarColors,
    )
}
