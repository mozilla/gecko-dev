/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.button.IconButton
import mozilla.components.compose.base.text.Text
import mozilla.components.compose.base.text.value
import mozilla.components.compose.base.textfield.TextField
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.ui.icons.R

/**
 * An eye trailing icon for a [TextField] that contains a password.
 * @param isPasswordVisible true when the password is revealed.
 * @param contentDescription the content description.
 * @param onTrailingIconClick invoked when pressing the eye icon
 */
@Composable
fun EyePasswordIconButton(
    isPasswordVisible: Boolean = false,
    contentDescription: Text? = null,
    onTrailingIconClick: () -> Unit,
) {
    IconButton(
        onClick = onTrailingIconClick,
        contentDescription = contentDescription?.value,
    ) {
        Icon(
            painter = if (!isPasswordVisible) {
                painterResource(id = R.drawable.mozac_ic_eye_24)
            } else {
                painterResource(id = R.drawable.mozac_ic_eye_slash_24)
            },
            contentDescription = null,
            tint = AcornTheme.colors.textPrimary,
        )
    }
}

@PreviewLightDark
@Composable
private fun EyePasswordIconButtonPreview() {
    var isPasswordVisible by remember { mutableStateOf(false) }

    AcornTheme {
        Column(
            modifier = Modifier
                .background(color = AcornTheme.colors.layer1)
                .padding(8.dp),
        ) {
            TextField(
                value = "password",
                onValueChange = {},
                isEnabled = true,
                placeholder = "",
                errorText = "",
                modifier = Modifier.fillMaxWidth(),
                label = "",
                trailingIcons = {
                    EyePasswordIconButton(
                        isPasswordVisible = isPasswordVisible,
                        onTrailingIconClick = { isPasswordVisible = !isPasswordVisible },
                    )
                },
                visualTransformation = if (isPasswordVisible) {
                    VisualTransformation.None
                } else {
                    PasswordVisualTransformation()
                },
            )
        }
    }
}
