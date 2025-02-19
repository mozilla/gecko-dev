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
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.ui.icons.R as iconsR

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(8.dp)

/**
 * Sub-component of the [BrowserToolbar] responsible for allowing the user to edit the current
 * URL ("edit mode").
 *
 * @param url The initial URL to be edited.
 * @param colors The color scheme to use in the browser edit toolbar.
 * @param onUrlEdit Will be called when the URL value changes. An updated text value comes as a
 * parameter of the callback.
 * @param onUrlCommitted Will be called when the user has finished editing and wants to initiate
 * loading the entered URL. The committed text value comes as a parameter of the callback.
 * @param editActions Optional actions to be displayed on the right side of the toolbar.
 */
@Composable
fun BrowserEditToolbar(
    url: String,
    colors: BrowserEditToolbarColors,
    onUrlEdit: (String) -> Unit = {},
    onUrlCommitted: (String) -> Unit = {},
    editActions: @Composable () -> Unit = {},
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
            trailingIcon = {
                editActions()

                if (url.isNotEmpty()) {
                    ClearButton(
                        tint = colors.clearButton,
                        onButtonClicked = { onUrlEdit("") },
                    )
                }
            },
        )
    }
}

/**
 * Sub-component of the [BrowserEditToolbar] responsible for displaying a clear icon button.
 *
 * @param tint Color tint of the clear button.
 * @param onButtonClicked Will be called when the user clicks on the button.
 */
@Composable
fun ClearButton(
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
        )
    }
}
