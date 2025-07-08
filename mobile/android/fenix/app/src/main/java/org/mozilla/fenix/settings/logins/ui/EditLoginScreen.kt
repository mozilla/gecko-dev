/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.IconButton
import mozilla.components.compose.base.textfield.TextField
import mozilla.components.compose.base.textfield.TextFieldColors
import mozilla.components.compose.base.textfield.TextFieldStyle
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

private val IconButtonHeight = 48.dp

@Composable
internal fun EditLoginScreen(store: LoginsStore) {
    val state by store.observeAsState(store.state) { it }
    val editState = state.loginsEditLoginState ?: return

    Scaffold(
        topBar = {
            EditLoginTopBar(
                store = store,
                loginItem = editState.login,
            )
        },
        containerColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .padding(paddingValues)
                .width(FirefoxTheme.layout.size.containerMaxWidth),
        ) {
            Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.static200))
            EditLoginUrl(url = editState.login.url)
            Spacer(modifier = Modifier.height(8.dp))
            EditLoginUsername(store = store, user = editState.login.username)
            Spacer(modifier = Modifier.height(8.dp))
            EditLoginPassword(store = store, pass = editState.login.password)
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun EditLoginTopBar(store: LoginsStore, loginItem: LoginItem) {
    val state by store.observeAsState(store.state.loginsEditLoginState) { it.loginsEditLoginState }
    val username = state?.newUsername ?: loginItem.username
    val password = state?.newPassword ?: loginItem.password

    val validModifiedUser = username.isNotBlank() && username != loginItem.username
    val validModifiedPassword = password.isNotBlank() && password != loginItem.password
    val isLoginValid = validModifiedUser || validModifiedPassword

    TopAppBar(
        colors = TopAppBarDefaults.topAppBarColors(containerColor = FirefoxTheme.colors.layer1),
        windowInsets = WindowInsets(
            top = 0.dp,
            bottom = 0.dp,
        ),
        title = {
            Text(
                text = stringResource(R.string.edit_2),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            IconButton(
                modifier = Modifier
                    .padding(horizontal = FirefoxTheme.layout.space.static50),
                onClick = { store.dispatch(EditLoginBackClicked) },
                contentDescription = stringResource(
                    R.string.edit_login_navigate_back_button_content_description,
                ),
            ) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
        actions = {
            Box {
                IconButton(
                    modifier = Modifier
                        .padding(horizontal = FirefoxTheme.layout.space.static50),
                    onClick = {
                        store.dispatch(
                            EditLoginAction.SaveEditClicked(loginItem),
                        )
                    },
                    contentDescription = stringResource(
                        R.string.edit_login_button_content_description,
                    ),
                    enabled = isLoginValid,
                ) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_checkmark_24),
                        contentDescription = null,
                        tint = if (isLoginValid) {
                            FirefoxTheme.colors.textPrimary
                        } else {
                            FirefoxTheme.colors.textDisabled
                        },
                    )
                }
            }
        },
    )
}

@Composable
private fun EditLoginUrl(url: String) {
    Text(
        text = stringResource(R.string.preferences_passwords_saved_logins_site),
        style = TextFieldStyle.default().labelStyle,
        color = TextFieldColors.default().labelColor,
        modifier = Modifier
            .padding(
                horizontal = FirefoxTheme.layout.space.static200,
            ),
    )

    Text(
        text = url,
        style = TextFieldStyle.default().placeholderStyle,
        color = FirefoxTheme.colors.textDisabled,
        modifier = Modifier
            .padding(
                horizontal = FirefoxTheme.layout.space.static200,
                vertical = FirefoxTheme.layout.space.static100,
            ),
    )
}

@Composable
private fun EditLoginUsername(store: LoginsStore, user: String) {
    val editState by store.observeAsState(store.state.loginsEditLoginState) { it.loginsEditLoginState }
    val username = editState?.newUsername ?: user

    TextField(
        value = username,
        onValueChange = { newUsername ->
            store.dispatch(EditLoginAction.UsernameChanged(newUsername))
        },
        placeholder = "",
        errorText = "",
        modifier = Modifier
            .padding(
                horizontal = FirefoxTheme.layout.space.static200,
                vertical = FirefoxTheme.layout.space.static100,
            ),
        label = stringResource(R.string.preferences_passwords_saved_logins_username),
        minHeight = IconButtonHeight,
        trailingIcons = {
            if (editState?.newUsername?.isNotEmpty() == true) {
                CrossTextFieldButton {
                    store.dispatch(EditLoginAction.UsernameChanged(""))
                }
            }
        },
        colors = TextFieldColors.default(
            placeholderColor = FirefoxTheme.colors.textPrimary,
        ),
    )
}

@Composable
private fun EditLoginPassword(store: LoginsStore, pass: String) {
    val editState by store.observeAsState(store.state.loginsEditLoginState) { it.loginsEditLoginState }
    val isPasswordVisible = editState?.isPasswordVisible ?: true
    val password = editState?.newPassword ?: pass

    Row(verticalAlignment = Alignment.CenterVertically) {
        TextField(
            value = password,
            onValueChange = { newPassword ->
                store.dispatch(EditLoginAction.PasswordChanged(newPassword))
            },
            placeholder = "",
            errorText = "",
            modifier = Modifier
                .padding(
                    horizontal = FirefoxTheme.layout.space.static200,
                    vertical = FirefoxTheme.layout.space.static100,
                ),
            label = stringResource(R.string.preferences_passwords_saved_logins_password),
            minHeight = IconButtonHeight,
            trailingIcons = {
                EyePasswordIconButton(
                    isPasswordVisible = isPasswordVisible,
                    onTrailingIconClick = {
                        store.dispatch(
                            EditLoginAction.PasswordVisibilityChanged(
                                !isPasswordVisible,
                            ),
                        )
                    },
                )
                if (editState?.newPassword?.isNotEmpty() == true) {
                    CrossTextFieldButton {
                        store.dispatch(EditLoginAction.PasswordChanged(""))
                    }
                }
            },
            visualTransformation = if (isPasswordVisible) {
                VisualTransformation.None
            } else {
                PasswordVisualTransformation()
            },
            colors = TextFieldColors.default(
                placeholderColor = FirefoxTheme.colors.textPrimary,
            ),
        )
    }
}

@Composable
@FlexibleWindowLightDarkPreview
private fun EditLoginScreenPreview() {
    val store = LoginsStore(
        initialState = LoginsState(
            loginItems = listOf(),
            searchText = "",
            sortOrder = LoginsSortOrder.default,
            biometricAuthenticationDialogState = null,
            loginsListState = null,
            loginsAddLoginState = null,
            loginsEditLoginState = LoginsEditLoginState(
                login = LoginItem(
                    guid = "123",
                    url = "https://www.justanothersite123.com",
                    username = "username 123",
                    password = "password 123",
                ),
                newUsername = "username 456",
                newPassword = "password 456",
                isPasswordVisible = true,
            ),
            loginsLoginDetailState = null,
            loginsDeletionState = null,
        ),
    )

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            EditLoginScreen(store)
        }
    }
}
