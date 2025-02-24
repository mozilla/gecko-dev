/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh.addexception

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.textfield.TextField
import org.mozilla.fenix.settings.doh.DohSettingsState
import org.mozilla.fenix.settings.doh.ProtectionLevel
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Composable function that displays the exceptions list screen of DoH settings.
 *
 * @param state The current [DohSettingsState].
 * @param onNavigateUp Invoked when the user clicks the navigate up (back) button.
 * @param onSaveClicked Invoked when the user clicks the save button.
 */
@Composable
internal fun AddExceptionScreen(
    state: DohSettingsState,
    onNavigateUp: () -> Unit = {},
    onSaveClicked: (String) -> Unit = {},
) {
    var urlInput by remember { mutableStateOf("") }
    Scaffold(
        topBar = {
            Toolbar(onToolbarBackClick = onNavigateUp)
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues),
        ) {
            TextField(
                value = urlInput,
                onValueChange = {
                    urlInput = it
                },
                placeholder = stringResource(R.string.preference_doh_exceptions_add_placeholder),
                errorText = stringResource(R.string.preference_doh_exceptions_add_error),
                label = stringResource(R.string.preference_doh_exceptions_add_site),
                isError = !state.isUserExceptionValid,
                singleLine = true,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(
                        vertical = 12.dp,
                        horizontal = 16.dp,
                    ),
            )

            PrimaryButton(
                text = stringResource(R.string.preference_doh_exceptions_add_save),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(
                        vertical = 12.dp,
                        horizontal = 16.dp,
                    ),
                onClick = { onSaveClicked(urlInput) },
            )
        }
    }
}

@Composable
private fun Toolbar(onToolbarBackClick: () -> Unit) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
                text = stringResource(R.string.preference_doh_exceptions_add),
            )
        },
        navigationIcon = {
            IconButton(onClick = onToolbarBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.preference_doh_up_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
    )
}

@Composable
@FlexibleWindowLightDarkPreview
private fun AddExceptionScreenPreview() {
    FirefoxTheme {
        AddExceptionScreen(
            state = DohSettingsState(
                allProtectionLevels = listOf(
                    ProtectionLevel.Default,
                    ProtectionLevel.Increased,
                    ProtectionLevel.Max,
                    ProtectionLevel.Off,
                ),
                selectedProtectionLevel = ProtectionLevel.Off,
                providers = emptyList(),
                selectedProvider = null,
                exceptionsList = emptyList(),
                isUserExceptionValid = false,
            ),
        )
    }
}
