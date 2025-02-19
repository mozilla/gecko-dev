/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.toolbar.compose

import androidx.compose.runtime.Composable
import mozilla.components.compose.browser.toolbar.BrowserDisplayToolbar
import mozilla.components.compose.browser.toolbar.BrowserEditToolbar
import mozilla.components.compose.browser.toolbar.BrowserToolbarColors
import mozilla.components.compose.browser.toolbar.BrowserToolbarDefaults

/**
 * A customizable toolbar for browsers.
 *
 * The toolbar can switch between two modes: display and edit. The display mode displays the current
 * URL and controls for navigation. In edit mode the current URL can be edited. Those two modes are
 * implemented by the [BrowserDisplayToolbar] and [BrowserEditToolbar] composables.
 *
 * @param onDisplayMenuClicked Invoked when the user clicks on the menu button in "display" mode.
 * @param onTextEdit Invoked when the user edits the text in the toolbar in "edit" mode.
 * @param onTextCommit Invoked when the user has finished editing the URL and wants
 * to commit the entered text.
 * @param onDisplayToolbarClick Invoked when the user clicks on the URL in "display" mode.
 * @param colors The color scheme the browser toolbar will use for the UI.
 * @param hint Text displayed in the toolbar when there's no URL to display (no tab or empty URL)
 * @param editMode Whether the toolbar is in "edit" or "display" mode.
 * @param editText The text the user is editing in "edit" mode.
 */
@Suppress("MagicNumber")
@Composable
fun BrowserToolbar(
    onDisplayMenuClicked: () -> Unit,
    onDisplayToolbarClick: () -> Unit,
    onTextEdit: (String) -> Unit,
    onTextCommit: (String) -> Unit,
    colors: BrowserToolbarColors = BrowserToolbarDefaults.colors(),
    url: String = "",
    hint: String = "",
    editMode: Boolean = false,
    editText: String? = null,
) {
    val input = when (editText) {
        null -> url
        else -> editText
    }

    if (editMode) {
        BrowserEditToolbar(
            url = input,
            colors = colors.editToolbarColors,
            onUrlCommitted = { text -> onTextCommit(text) },
            onUrlEdit = { text -> onTextEdit(text) },
        )
    } else {
        BrowserDisplayToolbar(
            url = url.takeIf { it.isNotEmpty() } ?: hint,
            colors = colors.displayToolbarColors,
            onUrlClicked = {
                onDisplayToolbarClick()
            },
            onMenuClicked = { onDisplayMenuClicked() },
        )
    }
}
