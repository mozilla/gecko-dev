/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material.Icon
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.IconButton
import mozilla.components.compose.base.menu.DropdownMenu
import mozilla.components.compose.base.menu.MenuItem
import mozilla.components.compose.base.textfield.TextField
import mozilla.components.compose.base.textfield.TextFieldColors
import mozilla.components.compose.base.textfield.TextFieldStyle
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

@Composable
internal fun LoginDetailsScreen(store: LoginsStore) {
    val state by store.observeAsState(store.state) { it }
    val detailState = state.loginsLoginDetailState ?: return

    Scaffold(
        topBar = {
            LoginDetailTopBar(
                store = store,
                loginItem = detailState.login,
                onBackClick = { store.dispatch(LoginsDetailBackClicked) },
            )
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .padding(paddingValues)
                .width(FirefoxTheme.layout.size.containerMaxWidth),
        ) {
            Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.static200))
            LoginDetailsUrl(store = store, url = detailState.login.url)
            Spacer(modifier = Modifier.height(8.dp))
            LoginDetailsUsername(store = store, username = detailState.login.username)
            Spacer(modifier = Modifier.height(8.dp))
            LoginDetailsPassword(store = store, password = detailState.login.password)
        }
    }
}

@Composable
private fun LoginDetailTopBar(
    store: LoginsStore,
    loginItem: LoginItem,
    onBackClick: () -> Unit,
) {
    var showMenu by remember { mutableStateOf(false) }

    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = loginItem.getDomainName(),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            androidx.compose.material.IconButton(onClick = onBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(
                        R.string.login_details_navigate_back_button_content_description,
                    ),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
        actions = {
            Box {
                IconButton(
                    onClick = { showMenu = true },
                    contentDescription = stringResource(
                        R.string.login_detail_menu_button_content_description,
                    ),
                    modifier = Modifier
                        .padding(horizontal = FirefoxTheme.layout.space.static50),
                    ) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                        contentDescription = null,
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }

                LoginDetailMenu(
                    showMenu = showMenu,
                    onDismissRequest = { showMenu = false },
                    loginItem = loginItem,
                    store = store,
                )
            }
        },
    )
}

@Composable
private fun LoginDetailMenu(
    showMenu: Boolean,
    onDismissRequest: () -> Unit,
    loginItem: LoginItem,
    store: LoginsStore,
) {
    DropdownMenu(
        menuItems = listOf(
            MenuItem.TextItem(
                text = mozilla.components.compose.base.text.Text.Resource(
                    R.string.login_detail_menu_edit_button,
                ),
                onClick = { store.dispatch(DetailLoginMenuAction.EditLoginMenuItemClicked(loginItem)) },
            ),
            MenuItem.TextItem(
                text = mozilla.components.compose.base.text.Text.Resource(
                    R.string.login_detail_menu_delete_button,
                ),
                onClick = {
                    store.dispatch(
                        DetailLoginMenuAction.DeleteLoginMenuItemClicked(
                            loginItem,
                        ),
                    )
                },
            ),
        ),
        expanded = showMenu,
        onDismissRequest = onDismissRequest,
    )
}

@Composable
private fun LoginDetailsUrl(store: LoginsStore, url: String) {
    Text(
        text = stringResource(R.string.preferences_passwords_saved_logins_site),
        style = TextFieldStyle.default().labelStyle,
        color = TextFieldColors.default().labelColor,
        modifier = Modifier
            .padding(
                horizontal = FirefoxTheme.layout.space.static200,
            ),
    )

    TextListItem(
        label = url,
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 16.dp)
            .wrapContentHeight(),
        iconPainter = painterResource(R.drawable.ic_open_in_new),
        iconDescription = stringResource(R.string.saved_login_open_site),
        onIconClick = { store.dispatch(DetailLoginAction.GoToSiteClicked(url)) },
    )
}

@Composable
private fun LoginDetailsUsername(store: LoginsStore, username: String) {
    Text(
        text = stringResource(R.string.preferences_passwords_saved_logins_username),
        style = TextFieldStyle.default().labelStyle,
        color = TextFieldColors.default().labelColor,
        modifier = Modifier
            .padding(
                start = 16.dp,
                end = 16.dp,
            ),
    )

    TextListItem(
        label = username,
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 16.dp)
            .wrapContentHeight(),
        iconPainter = painterResource(R.drawable.ic_copy),
        iconDescription = stringResource(R.string.saved_login_copy_username),
        onIconClick = { store.dispatch(DetailLoginAction.CopyUsernameClicked(username)) },
    )
}

@Composable
private fun LoginDetailsPassword(store: LoginsStore, password: String) {
    var isPasswordVisible by remember { mutableStateOf(false) }

    Text(
        text = stringResource(R.string.preferences_passwords_saved_logins_password),
        style = TextFieldStyle.default().labelStyle,
        color = TextFieldColors.default().labelColor,
        modifier = Modifier.padding(start = 16.dp),
    )

    Row(verticalAlignment = Alignment.CenterVertically) {
        TextField(
            value = password,
            onValueChange = {},
            isEnabled = false,
            placeholder = "",
            errorText = "",
            modifier = Modifier
                .padding(start = 32.dp)
                .weight(1f),
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

        IconButton(
            modifier = Modifier
                .padding(horizontal = FirefoxTheme.layout.space.static50)
                .size(48.dp),
            onClick = { store.dispatch(DetailLoginAction.CopyPasswordClicked(password)) },
            contentDescription = stringResource(R.string.saved_logins_copy_password),
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_copy),
                contentDescription = null,
                tint = AcornTheme.colors.textPrimary,
            )
        }
    }
}

@Composable
@FlexibleWindowLightDarkPreview
private fun LoginDetailsScreenPreview() {
    val store = LoginsStore(
        initialState = LoginsState(
            loginItems = listOf(),
            searchText = "",
            sortOrder = LoginsSortOrder.default,
            biometricAuthenticationDialogState = null,
            loginsListState = null,
            loginsAddLoginState = null,
            loginsEditLoginState = null,
            loginsLoginDetailState = LoginsLoginDetailState(
                login = LoginItem(
                    guid = "123",
                    url = "https://www.justanothersite123.com",
                    username = "username 123",
                    password = "password 123",
                ),
            ),
            loginsDeletionState = null,
        ),
    )

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            LoginDetailsScreen(store)
        }
    }
}
