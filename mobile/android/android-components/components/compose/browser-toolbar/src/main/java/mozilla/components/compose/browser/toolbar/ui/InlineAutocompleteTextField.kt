/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.text.InputType.TYPE_CLASS_TEXT
import android.text.InputType.TYPE_TEXT_VARIATION_URI
import android.util.TypedValue
import android.view.Gravity
import android.view.inputmethod.EditorInfo
import androidx.annotation.ColorInt
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.graphics.toColorInt
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.BrowserEditToolbar
import mozilla.components.support.ktx.android.view.showKeyboard
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText

private const val TEXT_SIZE = 15f
private const val TEXT_HIGHLIGHT_COLOR = "#5C592ACB"

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
    val context = LocalContext.current
    val textColor = AcornTheme.colors.textPrimary
    val hintColor = AcornTheme.colors.textSecondary
    val backgroundColor = AcornTheme.colors.layer3
    val backgroundDrawable = remember { buildBackground(context, backgroundColor.toArgb()) }
    val autocompletedTextColor = remember { TEXT_HIGHLIGHT_COLOR.toColorInt() }

    AndroidView(
        factory = { context ->
            InlineAutocompleteEditText(context).apply {
                imeOptions = EditorInfo.IME_ACTION_GO or
                    EditorInfo.IME_FLAG_NO_EXTRACT_UI or
                    EditorInfo.IME_FLAG_NO_FULLSCREEN
                inputType = TYPE_CLASS_TEXT or TYPE_TEXT_VARIATION_URI
                setLines(1)
                gravity = Gravity.CENTER_VERTICAL
                setTextSize(TypedValue.COMPLEX_UNIT_SP, TEXT_SIZE)
                setFocusable(true)
                background = backgroundDrawable
                autoCompleteBackgroundColor = autocompletedTextColor
                setTextColor(textColor.toArgb())
                setHintTextColor(hintColor.toArgb())

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

private fun buildBackground(
    context: Context,
    @ColorInt color: Int,
    cornerRadius: Float = 8f,
): GradientDrawable {
    val cornerRadiusPx = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP,
        cornerRadius,
        context.resources.displayMetrics,
    )

    return GradientDrawable().apply {
        shape = GradientDrawable.RECTANGLE
        setColor(color)
        this.cornerRadius = cornerRadiusPx
    }
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
