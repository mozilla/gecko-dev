/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.textfield

import androidx.compose.foundation.background
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material.TextFieldDefaults
import androidx.compose.material.TextFieldDefaults.indicatorLine
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.error
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.modifier.thenConditional
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.ui.icons.R

private val FocusedIndicatorLineThickness = 2.dp
private val UnfocusedIndicatorLineThickness = 1.dp

private val TrailingIconHeight = 24.dp

/**
 * UI for a text field.
 *
 * @param value The input text in the text field.
 * @param onValueChange The callback triggered when the input text field changes.
 * @param placeholder The text displayed when the text field is empty.
 * @param errorText The message displayed when there is an error.
 * @param modifier Modifier to be applied to the text field layout.
 * @param label Optional text displayed as a header above the input field.
 * @param isError Whether there is an error with the input value. When set to true, error styling
 * will be applied to the text field.
 * @param singleLine When set to true, this text field becomes a single horizontally scrolling text
 * field instead of wrapping onto multiple lines. Note that maxLines parameter will be ignored as
 * the maxLines attribute will be automatically set to 1.
 * @param maxLines The maximum number of input lines visible to the user at once.
 * @param minLines The minimum number of input lines visible to the user at once.
 * @param minHeight The minimum height constraint for the input field.
 * @param trailingIcons The optional composable for adding trailing icons at the end of the text field
 * container.
 * @param colors [TextFieldColors] to use for styling text field colors.
 * @param style [TextFieldStyle] to use for styling text field.
 * @param visualTransformation The visual transformation filter for changing the visual representation
 * of the input. By default no visual transformation is applied.
 * @param keyboardOptions Software keyboard options that contains configuration such as [KeyboardType] and [ImeAction].
 * @param keyboardActions When the input service emits an IME action, the corresponding callback is
 * called. Note that this IME action may be different from what you specified in
 * [KeyboardOptions.imeAction].
 */
@Suppress("LongMethod")
@OptIn(ExperimentalMaterialApi::class)
@Composable
fun TextField(
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    errorText: String,
    modifier: Modifier = Modifier,
    label: String? = null,
    isError: Boolean = false,
    singleLine: Boolean = true,
    maxLines: Int = if (singleLine) 1 else Int.MAX_VALUE,
    minLines: Int = 1,
    minHeight: Dp = TrailingIconHeight,
    trailingIcons: @Composable (TrailingIconScope.() -> Unit)? = null,
    colors: TextFieldColors = TextFieldColors.default(),
    style: TextFieldStyle = TextFieldStyle.default(),
    visualTransformation: VisualTransformation = VisualTransformation.None,
    keyboardOptions: KeyboardOptions = KeyboardOptions.Default,
    keyboardActions: KeyboardActions = KeyboardActions(),
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isEnabled = true

    // We use the Material textFieldColors for the indicator line as it keeps track of error
    // and focused states
    val indicatorLineColors = TextFieldDefaults.textFieldColors(
        focusedIndicatorColor = colors.focusedIndicatorColor,
        unfocusedIndicatorColor = colors.unfocusedIndicatorColor,
        errorIndicatorColor = colors.errorIndicatorColor,
    )

    Column(
        modifier = modifier,
    ) {
        BasicTextField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier
                .fillMaxWidth()
                .thenConditional(
                    modifier = Modifier.semantics { error(errorText) },
                ) { isError }
                .indicatorLine(
                    enabled = isEnabled,
                    isError = isError,
                    interactionSource = interactionSource,
                    colors = indicatorLineColors,
                    focusedIndicatorLineThickness = FocusedIndicatorLineThickness,
                    unfocusedIndicatorLineThickness = UnfocusedIndicatorLineThickness,
                )
                .defaultMinSize(
                    minWidth = TextFieldDefaults.MinWidth,
                ),
            enabled = isEnabled,
            textStyle = style.inputStyle.copy(
                color = colors.inputColor,
            ),
            cursorBrush = SolidColor(if (isError) colors.errorCursorColor else colors.cursorColor),
            visualTransformation = visualTransformation,
            keyboardOptions = keyboardOptions,
            keyboardActions = keyboardActions,
            interactionSource = interactionSource,
            singleLine = singleLine,
            maxLines = maxLines,
            minLines = minLines,
            decorationBox = @Composable { innerTextField ->
                Column(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    label?.let {
                        Text(
                            text = label,
                            style = style.labelStyle,
                            color = colors.labelColor,
                        )
                    }

                    Spacer(modifier = Modifier.height(4.dp))

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                // Ensures that the text field will remain the same height as the trailing icon
                                .heightIn(min = minHeight),
                            // The difference in alignment is to ensure that the placeholder text
                            // aligns with the cursor when more than 1 line is displayed
                            contentAlignment = if (singleLine || maxLines == 1) {
                                Alignment.CenterStart
                            } else {
                                Alignment.TopStart
                            },
                        ) {
                            // This only controls the cursor and the input text
                            innerTextField()

                            if (value.isEmpty()) {
                                Text(
                                    text = placeholder,
                                    style = style.placeholderStyle,
                                    color = colors.placeholderColor,
                                )
                            }
                        }

                        if (isError) {
                            Spacer(modifier = Modifier.width(12.dp))

                            Icon(
                                painter = painterResource(R.drawable.mozac_ic_warning_fill_24),
                                contentDescription = null,
                                tint = colors.errorTrailingIconColor,
                            )
                        } else if (trailingIcons != null) {
                            Spacer(modifier = Modifier.width(12.dp))

                            Row {
                                with(TrailingIconScope(this)) {
                                    trailingIcons.invoke(this)
                                }
                            }
                        }
                    }

                    // Padding between the text (and trailing icon), and the indicator line
                    Spacer(modifier = Modifier.height(4.dp))
                }
            },
        )

        if (isError) {
            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = errorText,
                // The a11y for this is handled via the above `BasicTextField`
                modifier = Modifier.clearAndSetSemantics { },
                style = style.errorTextStyle,
                color = colors.errorTextColor,
            )
        }
    }
}

/**
 * [Color]s to use for the cursor and indicator line of [TextField].
 *
 * @property inputColor The color for the input text.
 * @property labelColor The color for the label.
 * @property placeholderColor The color for the placeholder.
 * @property errorTextColor The color for the error text.
 * @property cursorColor The color for the cursor.
 * @property errorCursorColor The error color for the cursor.
 * @property focusedIndicatorColor The color for the indicator when focused.
 * @property unfocusedIndicatorColor The color for the indicator when not focused.
 * @property errorIndicatorColor The error color for the indicator.
 * @property errorTrailingIconColor The trailing icon color of the warning icon.
 */
data class TextFieldColors(
    val inputColor: Color,
    val labelColor: Color,
    val placeholderColor: Color,
    val errorTextColor: Color,
    val cursorColor: Color,
    val errorCursorColor: Color,
    val focusedIndicatorColor: Color,
    val unfocusedIndicatorColor: Color,
    val errorIndicatorColor: Color,
    val errorTrailingIconColor: Color,
) {

    /**
     * @see [TextFieldColors].
     */
    companion object {

        /**
         * The default colors for [TextField].
         *
         * @param inputColor @see [TextFieldColors.inputColor].
         * @param labelColor @see [TextFieldColors.labelColor].
         * @param placeholderColor @see [TextFieldColors.placeholderColor].
         * @param errorTextColor @see [TextFieldColors.errorTextColor].
         * @param cursorColor @see [TextFieldColors.cursorColor].
         * @param errorCursorColor @see [TextFieldColors.errorCursorColor].
         * @param focusedIndicatorColor @see [TextFieldColors.focusedIndicatorColor].
         * @param unfocusedIndicatorColor @see [TextFieldColors.unfocusedIndicatorColor].
         * @param errorIndicatorColor @see [TextFieldColors.errorIndicatorColor].
         * @param errorTrailingIconColor @see [TextFieldColors.errorTrailingIconColor].
         */
        @Composable
        fun default(
            inputColor: Color = AcornTheme.colors.textPrimary,
            labelColor: Color = AcornTheme.colors.textPrimary,
            placeholderColor: Color = AcornTheme.colors.textSecondary,
            errorTextColor: Color = AcornTheme.colors.textCritical,
            cursorColor: Color = AcornTheme.colors.borderFormDefault,
            errorCursorColor: Color = AcornTheme.colors.borderFormDefault,
            focusedIndicatorColor: Color = AcornTheme.colors.borderFormDefault,
            unfocusedIndicatorColor: Color = AcornTheme.colors.borderFormDefault,
            errorIndicatorColor: Color = AcornTheme.colors.borderCritical,
            errorTrailingIconColor: Color = AcornTheme.colors.iconCritical,
        ) = TextFieldColors(
            inputColor = inputColor,
            labelColor = labelColor,
            placeholderColor = placeholderColor,
            errorTextColor = errorTextColor,
            cursorColor = cursorColor,
            errorCursorColor = errorCursorColor,
            focusedIndicatorColor = focusedIndicatorColor,
            unfocusedIndicatorColor = unfocusedIndicatorColor,
            errorIndicatorColor = errorIndicatorColor,
            errorTrailingIconColor = errorTrailingIconColor,
        )
    }
}

/**
 * [TextStyle]s to use for the [TextField].
 *
 * @property inputStyle The text style for the input text.
 * @property labelStyle The text style for the label text.
 * @property placeholderStyle The text style for the placeholder text.
 * @property errorTextStyle The text style for the error text.
 */
data class TextFieldStyle(
    val inputStyle: TextStyle,
    val labelStyle: TextStyle,
    val placeholderStyle: TextStyle,
    val errorTextStyle: TextStyle,
) {

    /**
     * @see [TextFieldStyle].
     */
    companion object {

        /**
         * The default text styles for [TextField].
         *
         * @param inputStyle @see [TextFieldStyle.inputStyle].
         * @param labelStyle @see [TextFieldStyle.labelStyle].
         * @param placeholderStyle @see [TextFieldStyle.placeholderStyle].
         * @param errorTextStyle @see [TextFieldStyle.errorTextStyle].
         */
        fun default(
            inputStyle: TextStyle = AcornTheme.typography.subtitle1,
            labelStyle: TextStyle = AcornTheme.typography.caption,
            placeholderStyle: TextStyle = AcornTheme.typography.subtitle1,
            errorTextStyle: TextStyle = AcornTheme.typography.caption,
        ) = TextFieldStyle(
            inputStyle = inputStyle,
            labelStyle = labelStyle,
            placeholderStyle = placeholderStyle,
            errorTextStyle = errorTextStyle,
        )
    }
}

private data class TextFieldPreviewState(
    val initialText: String,
    val label: String,
    val placeholder: String = "Placeholder",
    val errorText: String = "Error text",
    val isError: Boolean = false,
    val singleLine: Boolean = true,
    val maxLines: Int = Int.MAX_VALUE,
    val minLines: Int = 1,
    val minHeight: Dp = TrailingIconHeight,
    val trailingIcons: @Composable (TrailingIconScope.() -> Unit)? = null,
)

private class TextFieldParameterProvider : PreviewParameterProvider<TextFieldPreviewState> {
    override val values: Sequence<TextFieldPreviewState>
        get() = sequenceOf(
            TextFieldPreviewState(
                initialText = "",
                label = "Empty, No error",
            ),
            TextFieldPreviewState(
                initialText = "",
                label = "Empty, Error",
                isError = true,
            ),
            TextFieldPreviewState(
                initialText = "Typed",
                label = "Typed, No error",
            ),
            TextFieldPreviewState(
                initialText = "Typed",
                label = "Typed, Error",
                isError = true,
            ),
            TextFieldPreviewState(
                initialText = "",
                label = "Empty, No error, Minimum lines is 2",
                singleLine = false,
                minLines = 2,
            ),
            TextFieldPreviewState(
                initialText = "",
                label = "Empty, Error, Minimum lines is 2",
                isError = true,
                singleLine = false,
                minLines = 2,
            ),
            TextFieldPreviewState(
                initialText = "",
                label = "Empty, No error, Maximum lines is 2",
                singleLine = false,
                maxLines = 2,
            ),
            TextFieldPreviewState(
                initialText = "",
                label = "Empty, Error, Maximum lines is 2",
                isError = true,
                singleLine = false,
                maxLines = 2,
            ),
            TextFieldPreviewState(
                initialText = "Typed",
                label = "Typed, No error, 1 trailing icon",
                minHeight = 48.dp,
                trailingIcons = { CrossTextFieldButton {} },
            ),
            TextFieldPreviewState(
                initialText = "Typed",
                label = "Typed, No error, 2 trailing icons",
                minHeight = 48.dp,
                trailingIcons = {
                    EyeTextFieldButton {}
                    CrossTextFieldButton {}
                },
            ),
            TextFieldPreviewState(
                initialText = "Typed",
                label = "Typed, Error, 2 trailing icons",
                isError = true,
                minHeight = 48.dp,
                trailingIcons = {
                    EyeTextFieldButton {}
                    CrossTextFieldButton {}
                },
            ),
        )
}

@Preview
@Composable
private fun TextFieldPreview(
    @PreviewParameter(TextFieldParameterProvider::class) textFieldState: TextFieldPreviewState,
) {
    var text by remember { mutableStateOf(textFieldState.initialText) }

    AcornTheme {
        Column(
            modifier = Modifier
                .background(color = AcornTheme.colors.layer1)
                .padding(8.dp),
        ) {
            TextField(
                value = text,
                onValueChange = { text = it },
                placeholder = textFieldState.placeholder,
                errorText = textFieldState.errorText,
                modifier = Modifier.fillMaxWidth(),
                label = textFieldState.label,
                isError = textFieldState.isError,
                singleLine = textFieldState.singleLine,
                maxLines = textFieldState.maxLines,
                minLines = textFieldState.minLines,
                minHeight = textFieldState.minHeight,
                trailingIcons = textFieldState.trailingIcons,
            )
        }
    }
}
