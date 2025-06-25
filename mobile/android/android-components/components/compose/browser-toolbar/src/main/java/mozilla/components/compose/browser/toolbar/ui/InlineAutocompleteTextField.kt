/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.os.Build
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
import mozilla.components.support.ktx.android.view.showKeyboard
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText

/**
 * Sub-component of the [BrowserEditToolbar] responsible for displaying a text field that is
 * capable of inline autocompletion.
 */
@Composable
internal fun InlineAutocompleteTextField(
    url: String,
    modifier: Modifier = Modifier,
    onUrlEdit: (String) -> Unit = {},
    onUrlCommitted: (String) -> Unit = {},
) {
    val textColor = AcornTheme.colors.textPrimary

    AndroidView(
        factory = { context ->
            InlineAutocompleteEditText(context).apply {
                inputType = TYPE_CLASS_TEXT or TYPE_TEXT_VARIATION_URI
                gravity = Gravity.CENTER_VERTICAL
                setLines(1)
                setFocusable(true)
                setTextColor(textColor.toArgb())

                updateText(url)

                setOnCommitListener {
                    onUrlCommitted(text.toString())
                }

                setOnTextChangeListener { text, _ ->
                    onUrlEdit(text)
                }
            }
        },
        modifier = modifier,
        update = {
            it.updateText(url)
        },
    )
}

private fun InlineAutocompleteEditText.updateText(newText: String) {
    // Avoid running the code for focusing this if the updated text is the one user already typed.
    // But ensure focusing this if just starting to type.
    if (text.toString() == newText && newText.isNotEmpty()) return

    setText(text = newText, shouldAutoComplete = false)
    setSelection(newText.length)
    if (!hasFocus()) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            // On Android 14 this needs to be called before requestFocus() in order to receive focus.
            isFocusableInTouchMode = true
        }
        requestFocus()
        showKeyboard()
    }
}

@PreviewLightDark
@Composable
private fun BrowserEditToolbarPreview() {
    InlineAutocompleteTextField(
        url = "http://www.mozilla.org",
    )
}
