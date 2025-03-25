/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.requiredSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.TextField
import androidx.compose.material.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.CustomAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.ui.InlineAutocompleteTextField
import mozilla.components.compose.browser.toolbar.ui.SearchSelector
import mozilla.components.ui.icons.R as iconsR

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(8.dp)

/**
 * Sub-component of the [BrowserToolbar] responsible for allowing the user to edit the current
 * URL ("edit mode").
 *
 * @param url The initial URL to be edited.
 * @param colors The color scheme to use in the browser edit toolbar.
 * @param useComposeTextField Whether or not to use the Compose [TextField] or a view-based
 * inline autocomplete text field.
 * @param editActionsStart List of [Action]s to be displayed at the start of the URL of
 * the edit toolbar.
 * @param editActionsEnd List of [Action]s to be displayed at the end of the URL of
 * the edit toolbar.
 * @param onUrlEdit Will be called when the URL value changes. An updated text value comes as a
 * parameter of the callback.
 * @param onUrlCommitted Will be called when the user has finished editing and wants to initiate
 * loading the entered URL. The committed text value comes as a parameter of the callback.
 * @param onInteraction Callback for handling [BrowserToolbarEvent]s on user interactions.
 */
@Composable
@Suppress("LongMethod")
fun BrowserEditToolbar(
    url: String,
    colors: BrowserEditToolbarColors,
    useComposeTextField: Boolean = false,
    editActionsStart: List<Action> = emptyList(),
    editActionsEnd: List<Action> = emptyList(),
    onUrlEdit: (String) -> Unit = {},
    onUrlCommitted: (String) -> Unit = {},
    onInteraction: (BrowserToolbarEvent) -> Unit,
) {
    Row(
        modifier = Modifier
            .background(color = colors.background)
            .padding(all = 8.dp)
            .background(
                color = colors.urlBackground,
                shape = ROUNDED_CORNER_SHAPE,
            ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (useComposeTextField) {
            TextField(
                value = url,
                onValueChange = { value ->
                    onUrlEdit(value)
                },
                colors = TextFieldDefaults.textFieldColors(
                    textColor = colors.text,
                    focusedIndicatorColor = Color.Transparent,
                    unfocusedIndicatorColor = Color.Transparent,
                    disabledIndicatorColor = Color.Transparent,
                    errorIndicatorColor = Color.Transparent,
                ),
                singleLine = true,
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Uri,
                    imeAction = ImeAction.Go,
                ),
                keyboardActions = KeyboardActions(
                    onGo = { onUrlCommitted(url) },
                ),
                modifier = Modifier.fillMaxWidth(),
                shape = ROUNDED_CORNER_SHAPE,
                leadingIcon = {
                    ActionContainer(
                        actions = editActionsStart,
                        onInteraction = onInteraction,
                    )
                },
                trailingIcon = {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        ActionContainer(
                            actions = editActionsEnd,
                            onInteraction = onInteraction,
                        )

                        if (url.isNotEmpty()) {
                            ClearButton(
                                tint = colors.clearButton,
                                onButtonClicked = { onUrlEdit("") },
                            )
                        }
                    }
                },
            )
        } else {
            ActionContainer(
                actions = editActionsStart,
                onInteraction = onInteraction,
            )

            InlineAutocompleteTextField(
                url = url,
                colors = colors,
                modifier = Modifier.weight(1f),
                onUrlEdit = onUrlEdit,
                onUrlCommitted = onUrlCommitted,
            )

            ActionContainer(
                actions = editActionsEnd,
                onInteraction = onInteraction,
            )

            if (url.isNotEmpty()) {
                ClearButton(
                    tint = colors.clearButton,
                    onButtonClicked = { onUrlEdit("") },
                )
            }
        }
    }
}

/**
 * Sub-component of the [BrowserEditToolbar] responsible for displaying a clear icon button.
 *
 * @param tint Color tint of the clear button.
 * @param onButtonClicked Will be called when the user clicks on the button.
 */
@Composable
private fun ClearButton(
    tint: Color,
    onButtonClicked: () -> Unit = {},
) {
    IconButton(
        modifier = Modifier.requiredSize(40.dp),
        onClick = { onButtonClicked() },
    ) {
        Icon(
            painter = painterResource(iconsR.drawable.mozac_ic_cross_circle_fill_24),
            contentDescription = stringResource(R.string.mozac_clear_button_description),
            tint = tint,
        )
    }
}

@PreviewLightDark
@Composable
private fun BrowserEditToolbarPreview() {
    AcornTheme {
        BrowserEditToolbar(
            url = "http://www.mozilla.org",
            colors = BrowserToolbarDefaults.colors().editToolbarColors,
            useComposeTextField = true,
            editActionsStart = listOf(
                CustomAction(
                    content = {
                        SearchSelector(
                            painter = painterResource(iconsR.drawable.mozac_ic_search_24),
                            tint = AcornTheme.colors.iconPrimary,
                            onClick = {},
                        )
                    },
                ),
            ),
            editActionsEnd = listOf(
                ActionButton(
                    icon = iconsR.drawable.mozac_ic_microphone_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
                ActionButton(
                    icon = iconsR.drawable.mozac_ic_qr_code_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
            ),
            onInteraction = {},
        )
    }
}
