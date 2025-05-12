/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.ui

import androidx.compose.ui.graphics.Color

/**
 * @property title The title text to display in the Toolbar.
 * @property backgroundColor The background color of the Toolbar.
 * @property textColor The color of the text (title) in the Toolbar.
 * @property iconColor The color of the icons (e.g., navigation icon, overflow icon) in the Toolbar.
 */
data class ToolbarConfig(
    val title: String,
    val backgroundColor: Color,
    val textColor: Color,
    val iconColor: Color,
)
