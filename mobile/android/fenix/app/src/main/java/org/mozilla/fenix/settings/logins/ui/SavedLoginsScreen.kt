/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Icon
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.CollectionInfo
import androidx.compose.ui.semantics.collectionInfo
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.IconButton
import mozilla.components.compose.base.menu.DropdownMenu
import mozilla.components.compose.base.menu.MenuItem
import mozilla.components.compose.base.textfield.TextField
import mozilla.components.compose.base.textfield.TextFieldColors
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.support.ktx.kotlin.trimmed
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.compose.list.SelectableFaviconListItem
import org.mozilla.fenix.settings.logins.ui.LoginsSortOrder.Alphabetical.isGuidToDelete
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The UI host for the Saved Logins list screen and related sub screens.
 *
 * @param buildStore A builder function to construct a [LoginsStore] using the NavController that's local
 * to the nav graph for the Logins view hierarchy.
 * @param startDestination the screen on which to initialize [SavedLoginsScreen] with.
 */
@Composable
internal fun SavedLoginsScreen(
    buildStore: (NavHostController) -> LoginsStore,
    startDestination: String = LoginsDestinations.LIST,
) {
    val navController = rememberNavController()
    val store = buildStore(navController)

    DisposableEffect(LocalLifecycleOwner.current) {
        onDispose {
            store.dispatch(ViewDisposed)
        }
    }

    NavHost(
        navController = navController,
        startDestination = startDestination,
    ) {
        composable(route = LoginsDestinations.LIST) {
            BackHandler { store.dispatch(LoginsListBackClicked) }
            LoginsList(store = store)
        }
        composable(route = LoginsDestinations.ADD_LOGIN) {
            BackHandler { store.dispatch(AddLoginBackClicked) }
            AddLoginScreen(store = store)
        }
        composable(route = LoginsDestinations.EDIT_LOGIN) {
            BackHandler { store.dispatch(EditLoginAction.BackEditClicked) }
        }
        composable(route = LoginsDestinations.LOGIN_DETAILS) {
            BackHandler { store.dispatch(LoginsDetailBackClicked) }
            LoginDetailsScreen(store = store)
        }
    }
}

internal object LoginsDestinations {
    const val LIST = "list"
    const val ADD_LOGIN = "add login"
    const val EDIT_LOGIN = "edit login"
    const val LOGIN_DETAILS = "login details"
}

@Composable
private fun LoginsList(store: LoginsStore) {
    val state by store.observeAsState(store.state) { it }

    Scaffold(
        topBar = {
            LoginsListTopBar(
                store = store,
                text = state.searchText ?: "",
            )
        },
        containerColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->

        if (state.searchText.isNullOrEmpty() && state.loginItems.isEmpty()) {
            EmptyList(dispatcher = store::dispatch)
            return@Scaffold
        }

        LazyColumn(
            modifier = Modifier
                .padding(paddingValues)
                .padding(vertical = 16.dp)
                .semantics {
                    collectionInfo =
                        CollectionInfo(rowCount = state.loginItems.size, columnCount = 1)
                },
        ) {
            itemsIndexed(state.loginItems) { _, item ->

                if (state.isGuidToDelete(item.guid)) {
                    return@itemsIndexed
                }

                SelectableFaviconListItem(
                    label = item.url.trimmed(),
                    url = item.url,
                    isSelected = false,
                    onClick = { store.dispatch(LoginClicked(item)) },
                    description = item.username.trimmed(),
                )
            }

            item {
                AddPasswordItem(
                    onAddPasswordClicked = { store.dispatch(AddLoginAction.InitAdd) },
                )
            }
        }
    }
}

@Composable
private fun AddPasswordItem(
    onAddPasswordClicked: () -> Unit,
) {
    IconListItem(
        label = stringResource(R.string.preferences_logins_add_login_2),
        beforeIconPainter = painterResource(R.drawable.ic_new),
        onClick = { onAddPasswordClicked() },
    )
}

@Composable
@Suppress("MaxLineLength")
private fun EmptyList(
    dispatcher: (LoginsAction) -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .fillMaxSize(),
        contentAlignment = Alignment.TopStart,
    ) {
        Column(
            horizontalAlignment = Alignment.Start,
            verticalArrangement = Arrangement.spacedBy(16.dp),
            modifier = modifier.padding(16.dp),
        ) {
            Text(
                text = String.format(
                    stringResource(R.string.preferences_passwords_saved_logins_description_empty_text_2),
                    stringResource(R.string.app_name),
                ),
                style = FirefoxTheme.typography.body2,
                color = FirefoxTheme.colors.textPrimary,
            )

            LinkText(
                text = stringResource(R.string.preferences_passwords_saved_logins_description_empty_learn_more_link_2),
                linkTextStates = listOf(
                    LinkTextState(
                        text = stringResource(R.string.preferences_passwords_saved_logins_description_empty_learn_more_link_2),
                        url = "",
                        onClick = { dispatcher(LearnMoreAboutSync) },
                    ),
                ),
                style = FirefoxTheme.typography.body2.copy(
                    color = FirefoxTheme.colors.textPrimary,
                ),
                linkTextColor = FirefoxTheme.colors.textPrimary,
                linkTextDecoration = TextDecoration.Underline,
            )

            AddPasswordItem(
                onAddPasswordClicked = { dispatcher(AddLoginAction.InitAdd) },
            )
        }
    }
}

@Composable
@Suppress("LongMethod")
private fun LoginsListTopBar(
    store: LoginsStore,
    text: String,
) {
    var showMenu by remember { mutableStateOf(false) }
    var searchActive by remember { mutableStateOf(false) }

    val iconColor = FirefoxTheme.colors.iconPrimary

    Box {
        TopAppBar(
            backgroundColor = FirefoxTheme.colors.layer1,
            title = {
                if (!searchActive) {
                    Text(
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.headline6,
                        text = stringResource(R.string.preferences_passwords_saved_logins_2),
                    )
                }
            },
            navigationIcon = {
                IconButton(
                    onClick = {
                        if (!searchActive) {
                            store.dispatch(LoginsListBackClicked)
                        } else {
                            searchActive = false
                        }
                    },
                    contentDescription = null,
                ) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_back_24),
                        contentDescription = stringResource(R.string.logins_navigate_back_button_content_description),
                        tint = iconColor,
                    )
                }
            },
            actions = {
                if (!searchActive) {
                    Box {
                        Icon(
                            modifier = Modifier
                                .clickable {
                                    showMenu = true
                                },
                            painter = if (showMenu) {
                                painterResource(R.drawable.ic_chevron_up)
                            } else {
                                painterResource(R.drawable.ic_chevron_down)
                            },
                            contentDescription = stringResource(
                                R.string.saved_logins_menu_dropdown_chevron_icon_content_description_2,
                            ),
                            tint = iconColor,
                        )

                        LoginListSortMenu(
                            showMenu = showMenu,
                            onDismissRequest = {
                                showMenu = false
                            },
                            store = store,
                        )
                    }
                    IconButton(onClick = { searchActive = true }, contentDescription = null) {
                        Icon(
                            painter = painterResource(R.drawable.ic_search),
                            contentDescription = stringResource(R.string.preferences_passwords_saved_logins_search_2),
                            tint = iconColor,
                        )
                    }
                } else {
                    Box {
                        TextField(
                            value = text,
                            placeholder = stringResource(R.string.preferences_passwords_saved_logins_search_2),
                            onValueChange = {
                                store.dispatch(SearchLogins(it, store.state.loginItems))
                            },
                            errorText = "",
                            modifier = Modifier
                                .background(color = FirefoxTheme.colors.layer1)
                                .fillMaxWidth(),
                            trailingIcons = {
                                if (text.isNotBlank()) {
                                    IconButton(
                                        onClick = {
                                            store.dispatch(
                                                SearchLogins(
                                                    "",
                                                    store.state.loginItems,
                                                ),
                                            )
                                        },
                                        contentDescription = null,
                                    ) {
                                        Icon(
                                            painter = painterResource(R.drawable.mozac_ic_cross_24),
                                            contentDescription = null,
                                            tint = iconColor,
                                        )
                                    }
                                }
                            },
                            colors = TextFieldColors.default(
                                placeholderColor = FirefoxTheme.colors.textPrimary,
                                cursorColor = Color.DarkGray,
                            ),
                            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
                        )
                    }
                }
            },
        )
    }
}

@Composable
private fun LoginListSortMenu(
    showMenu: Boolean,
    onDismissRequest: () -> Unit,
    store: LoginsStore,
) {
    val sortOrder by store.observeAsState(store.state.sortOrder) { store.state.sortOrder }
    DropdownMenu(
        menuItems = listOf(
            MenuItem.CheckableItem(
                text = mozilla.components.compose.base.text.Text.Resource(
                    R.string.saved_logins_sort_strategy_alphabetically,
                ),
                onClick = { store.dispatch(LoginsListSortMenuAction.OrderByNameClicked) },
                isChecked = sortOrder == LoginsSortOrder.Alphabetical,
            ),
            MenuItem.CheckableItem(
                text = mozilla.components.compose.base.text.Text.Resource(
                    R.string.saved_logins_sort_strategy_last_used,
                ),
                onClick = { store.dispatch(LoginsListSortMenuAction.OrderByLastUsedClicked) },
                isChecked = sortOrder == LoginsSortOrder.LastUsed,
            ),
        ),
        expanded = showMenu,
        onDismissRequest = onDismissRequest,
    )
}

private const val LOGINS_LIST_SIZE = 15

@Composable
@FlexibleWindowLightDarkPreview
private fun LoginsListScreenPreview() {
    val loginItems = List(LOGINS_LIST_SIZE) {
        LoginItem(
            guid = "$it",
            url = "https://www.justanothersite$it.com",
            username = "username $it",
            password = "password $it",
        )
    }

    val store = { _: NavHostController ->
        LoginsStore(
            initialState = LoginsState(
                loginItems = loginItems,
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
    }

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            SavedLoginsScreen(store)
        }
    }
}

@Composable
@FlexibleWindowLightDarkPreview
private fun EmptyLoginsListScreenPreview() {
    val store = { _: NavHostController ->
        LoginsStore(
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
    }

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            SavedLoginsScreen(store)
        }
    }
}
