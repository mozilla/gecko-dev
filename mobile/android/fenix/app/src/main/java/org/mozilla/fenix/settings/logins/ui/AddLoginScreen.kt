/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import android.util.Patterns
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.IconButton
import mozilla.components.compose.base.textfield.TextField
import mozilla.components.compose.base.textfield.TextFieldColors
import mozilla.components.compose.base.textfield.TextFieldStyle
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.support.ktx.util.URLStringUtils.isHttpOrHttps
import mozilla.components.support.ktx.util.URLStringUtils.isValidHost
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

private val IconButtonHeight = 48.dp

@Composable
internal fun AddLoginScreen(store: LoginsStore) {
    Scaffold(
        topBar = {
            AddLoginTopBar(store)
        },
        containerColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .padding(paddingValues)
                .width(FirefoxTheme.layout.size.containerMaxWidth),
        ) {
            Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.static200))
            AddLoginHost(store = store)
            Spacer(modifier = Modifier.height(8.dp))
            AddLoginUsername(store = store)
            Spacer(modifier = Modifier.height(8.dp))
            AddLoginPassword(store = store)
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AddLoginTopBar(store: LoginsStore) {
    val state by store.observeAsState(store.state.loginsAddLoginState) { it.loginsAddLoginState }
    val host = state?.host ?: ""
    val username = state?.username ?: ""
    val password = state?.password ?: ""
    val isLoginValid = isValidHost(host) && username.isNotBlank() && password.isNotBlank()

    TopAppBar(
        colors = TopAppBarDefaults.topAppBarColors(containerColor = FirefoxTheme.colors.layer1),
        windowInsets = WindowInsets(
            top = 0.dp,
            bottom = 0.dp,
        ),
        title = {
            Text(
                text = stringResource(R.string.add_login_2),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            IconButton(
                modifier = Modifier
                    .padding(horizontal = FirefoxTheme.layout.space.static50),
                onClick = { store.dispatch(AddLoginBackClicked) },
                contentDescription = stringResource(
                    R.string.add_login_navigate_back_button_content_description,
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
                            AddLoginAction.AddLoginSaveClicked,
                        )
                    },
                    contentDescription = stringResource(
                        R.string.add_login_save_new_login_button_content_description,
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
private fun AddLoginHost(store: LoginsStore) {
    val state by store.observeAsState(store.state.loginsAddLoginState) { it.loginsAddLoginState }
    val host = state?.host ?: ""
    var isFocused by remember { mutableStateOf(false) }

    TextField(
        value = host,
        onValueChange = { newHost ->
            store.dispatch(AddLoginAction.HostChanged(newHost))
        },
        placeholder = stringResource(R.string.add_login_hostname_hint_text),
        errorText = stringResource(R.string.add_login_hostname_invalid_text_2),
        isError = host.isNotBlank() && !Patterns.WEB_URL.matcher(host).matches(),
        modifier = Modifier
            .onFocusChanged { isFocused = it.isFocused }
            .padding(
                horizontal = FirefoxTheme.layout.space.static200,
                vertical = FirefoxTheme.layout.space.static100,
            ),
        label = stringResource(R.string.preferences_passwords_saved_logins_site),
        minHeight = IconButtonHeight,
        trailingIcons = {
            if (isFocused && isValidHost(host)) {
                CrossTextFieldButton { store.dispatch(AddLoginAction.HostChanged("")) }
            }
        },
    )

    if ((host.isEmpty() || isValidHost(host)) && !isHttpOrHttps(host)) {
        Spacer(modifier = Modifier.height(4.dp))

        Text(
            text = stringResource(R.string.add_login_hostname_invalid_text_3),
            modifier = Modifier.padding(horizontal = FirefoxTheme.layout.space.static200),
            style = TextFieldStyle.default().labelStyle,
            color = TextFieldColors.default().placeholderColor,
        )
    }
}

@Composable
private fun AddLoginUsername(store: LoginsStore) {
    val addLoginState by store.observeAsState(store.state.loginsAddLoginState) { it.loginsAddLoginState }
    val newLoginState by store.observeAsState(store.state.newLoginState) { it.newLoginState }
    val username = addLoginState?.username ?: ""
    var isFocused by remember { mutableStateOf(false) }

    TextField(
        value = username,
        onValueChange = { newUsername ->
            store.dispatch(AddLoginAction.UsernameChanged(newUsername))
        },
        placeholder = "",
        errorText = stringResource(R.string.saved_login_duplicate),
        isError = newLoginState == NewLoginState.Duplicate,
        modifier = Modifier
            .onFocusChanged { isFocused = it.isFocused }
            .padding(
                horizontal = FirefoxTheme.layout.space.static200,
                vertical = FirefoxTheme.layout.space.static100,
            ),
        label = stringResource(R.string.preferences_passwords_saved_logins_username),
        minHeight = IconButtonHeight,
        trailingIcons = {
            if (isFocused && addLoginState?.username?.isNotEmpty() == true) {
                CrossTextFieldButton { store.dispatch(AddLoginAction.UsernameChanged("")) }
            }
        },
        colors = TextFieldColors.default(
            placeholderColor = FirefoxTheme.colors.textPrimary,
        ),
    )
}

@Composable
private fun AddLoginPassword(store: LoginsStore) {
    val state by store.observeAsState(store.state.loginsAddLoginState) { it.loginsAddLoginState }
    val password = state?.password ?: ""
    var isFocused by remember { mutableStateOf(false) }

    TextField(
        value = password,
        onValueChange = { newPassword ->
            store.dispatch(AddLoginAction.PasswordChanged(newPassword))
        },
        placeholder = "",
        errorText = stringResource(R.string.saved_login_password_required_2),
        isError = isFocused && password.isBlank(),
        modifier = Modifier
            .onFocusChanged { isFocused = it.isFocused }
            .padding(
                horizontal = FirefoxTheme.layout.space.static200,
                vertical = FirefoxTheme.layout.space.static100,
            ),
        label = stringResource(R.string.preferences_passwords_saved_logins_password),
        minHeight = IconButtonHeight,
        trailingIcons = {
            if (isFocused && state?.password?.isNotEmpty() == true) {
                CrossTextFieldButton { store.dispatch(AddLoginAction.PasswordChanged("")) }
            }
        },
        colors = TextFieldColors.default(
            placeholderColor = FirefoxTheme.colors.textPrimary,
        ),
        visualTransformation = PasswordVisualTransformation(),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
    )
}

@Composable
@FlexibleWindowLightDarkPreview
private fun AddLoginScreenPreview() {
    val store = LoginsStore(
        initialState = LoginsState(
            loginItems = listOf(),
            searchText = "",
            sortOrder = LoginsSortOrder.default,
            biometricAuthenticationDialogState = null,
            loginsListState = null,
            loginsAddLoginState = null,
            loginsEditLoginState = null,
            loginsLoginDetailState = null,
            loginsDeletionState = null,
        ),
    )

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            AddLoginScreen(store)
        }
    }
}
