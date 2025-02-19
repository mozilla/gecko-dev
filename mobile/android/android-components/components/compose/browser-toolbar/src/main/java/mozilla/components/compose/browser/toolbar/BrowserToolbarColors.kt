/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.ui.graphics.Color

/**
 * Represents the colors used by the browser toolbar.
 *
 * @property displayToolbarColors The color scheme to use in browser display toolbar.
 * @property editToolbarColors The color scheme to use in the browser edit toolbar.
 */
data class BrowserToolbarColors(
    val displayToolbarColors: BrowserDisplayToolbarColors,
    val editToolbarColors: BrowserEditToolbarColors,
)

/**
 * Represents the colors used by the browser display toolbar.
 *
 * @property background Background color of the toolbar.
 * @property text Text color of the URL.
 */
data class BrowserDisplayToolbarColors(
    val background: Color,
    val text: Color,
)

/**
 * Represents the colors used by the browser edit toolbar.
 *
 * @property background Background color of the toolbar.
 * @property text Text color of the URL.
 */
data class BrowserEditToolbarColors(
    val background: Color,
    val text: Color,
)
