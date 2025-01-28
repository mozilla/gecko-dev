/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.textfield

import androidx.annotation.DrawableRes
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.text.Text
import org.mozilla.fenix.compose.text.value
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Scope for [TextField] trailing icons.
 */
class TrailingIconScope(rowScope: RowScope) : RowScope by rowScope {

    /**
     * An eye [TextField] trailing icon.
     */
    @Composable
    fun EyeTextFieldButton(
        contentDescription: Text? = Text.Resource(R.string.text_field_eye_trailing_icon_default_content_description),
        onTrailingIconClick: () -> Unit,
    ) = TrailingIconButton(
        iconId = R.drawable.mozac_ic_eye_24,
        contentDescription = contentDescription,
        tint = FirefoxTheme.colors.textPrimary,
        onTrailingIconClick = onTrailingIconClick,
    )

    /**
     * A cross [TextField] trailing icon.
     */
    @Composable
    fun CrossTextFieldButton(
        contentDescription: Text? = Text.Resource(R.string.text_field_cross_trailing_icon_default_content_description),
        onTrailingIconClick: () -> Unit,
    ) = TrailingIconButton(
        iconId = R.drawable.mozac_ic_cross_circle_fill_24,
        contentDescription = contentDescription,
        tint = FirefoxTheme.colors.textPrimary,
        onTrailingIconClick = onTrailingIconClick,
    )

    @Composable
    private fun TrailingIconButton(
        @DrawableRes iconId: Int,
        contentDescription: Text?,
        tint: Color,
        onTrailingIconClick: () -> Unit,
    ) {
        IconButton(onClick = onTrailingIconClick) {
            Icon(
                painter = painterResource(id = iconId),
                contentDescription = contentDescription?.value,
                tint = tint,
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun EyeTextFieldButtonPreview() {
    var textFieldInput by remember { mutableStateOf("password") }
    var isPasswordVisible by remember { mutableStateOf(false) }

    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(8.dp),
        ) {
            TextField(
                value = textFieldInput,
                onValueChange = {
                    textFieldInput = it
                },
                placeholder = "",
                errorText = "",
                label = "Eye",
                trailingIcons = { EyeTextFieldButton { isPasswordVisible = !isPasswordVisible } },
                visualTransformation = if (isPasswordVisible) {
                    VisualTransformation.None
                } else {
                    PasswordVisualTransformation()
                },
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun CrossTextFieldButtonPreview() {
    var textFieldInput by remember { mutableStateOf("Delete me") }

    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(8.dp),
        ) {
            TextField(
                value = textFieldInput,
                onValueChange = {
                    textFieldInput = it
                },
                placeholder = "",
                errorText = "",
                label = "Cross",
                trailingIcons = { CrossTextFieldButton { textFieldInput = "" } },
            )
        }
    }
}
