/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.text.InputType.TYPE_CLASS_TEXT
import android.text.InputType.TYPE_TEXT_VARIATION_URI
import android.view.Gravity
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.viewinterop.AndroidView
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.BrowserEditToolbar
import mozilla.components.compose.browser.toolbar.BrowserEditToolbarColors
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText

/**
 * Sub-component of the [BrowserEditToolbar] responsible for displaying a text field that is
 * capable of inline autocompletion.
 */
@Composable
internal fun InlineAutocompleteTextField(
    url: String,
    colors: BrowserEditToolbarColors,
    modifier: Modifier = Modifier,
    onUrlEdit: (String) -> Unit = {},
    onUrlCommitted: (String) -> Unit = {},
) {
    AndroidView(
        factory = { context ->
            InlineAutocompleteEditText(context).apply {
                inputType = TYPE_CLASS_TEXT or TYPE_TEXT_VARIATION_URI
                gravity = Gravity.CENTER_VERTICAL
                setLines(1)
                setTextColor(colors.text.toArgb())

                setText(text = url, shouldAutoComplete = false)

                setOnCommitListener {
                    onUrlCommitted(text.toString())
                }

                setOnTextChangeListener { text, _ ->
                    onUrlEdit(text)
                }
            }
        },
        modifier = modifier,
    )
}

@PreviewLightDark
@Composable
private fun BrowserEditToolbarPreview() {
    AcornTheme {
        InlineAutocompleteTextField(
            url = "http://www.mozilla.org",
            colors = BrowserEditToolbarColors(
                background = AcornTheme.colors.layer1,
                urlBackground = AcornTheme.colors.layer3,
                text = AcornTheme.colors.textPrimary,
                clearButton = AcornTheme.colors.iconPrimary,
            ),
        )
    }
}
