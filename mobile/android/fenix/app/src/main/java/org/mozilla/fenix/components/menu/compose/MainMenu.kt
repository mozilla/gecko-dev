/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.compose.cfr.CFRPopup
import mozilla.components.compose.cfr.CFRPopupLayout
import mozilla.components.compose.cfr.CFRPopupProperties
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.displayName
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.service.fxa.store.Account
import mozilla.components.service.fxa.store.SyncStore
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.menu.compose.header.MenuHeader
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

private const val ARROW_VERTICAL_OFFSET = 10
private const val INDICATOR_START_OFFSET = 46

/**
 * Wrapper composable to display a Contextual Feature Recommendation popup on [MainMenu]
 *
 * @param accessPoint The [MenuAccessPoint] that was used to navigate to the menu dialog.
 * @param store The [MenuStore] that is used for the current state.
 * @param syncStore The [SyncStore] used to determine account information.
 * @param installedAddons A list of installed [Addon]s to be shown.
 * @param showQuitMenu Whether or not to show the [QuitMenuGroup].
 * @param isPrivate Whether or not the browsing mode is in private mode.
 * @param isDesktopMode Whether or not the current site is in desktop mode.
 * @param isPdf Whether or not the current tab is a PDF.
 * @param isTranslationSupported Whether or not Translations are supported.
 * @param isExtensionsProcessDisabled Whether or not the extensions process is disabled due to extension errors.
 * @param onExtensionsMenuClick Invoked when the user clicks on extensions menu button.
 * @param onToolsMenuClick Invoked when the user clicks on tools menu button.
 * @param onSaveMenuClick Invoked when the user clicks on save menu button.
 */
@Suppress("LongParameterList")
@Composable
internal fun MainMenuWithCFR(
    accessPoint: MenuAccessPoint,
    store: MenuStore,
    syncStore: SyncStore,
    installedAddons: List<Addon>,
    showQuitMenu: Boolean,
    isPrivate: Boolean,
    isDesktopMode: Boolean,
    isPdf: Boolean,
    isTranslationSupported: Boolean,
    isExtensionsProcessDisabled: Boolean,
    onExtensionsMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
) {
    val configuration = LocalConfiguration.current
    val screenWidth = configuration.screenWidthDp
    val indicatorArrowStartOffset = (screenWidth / 2) - INDICATOR_START_OFFSET

    CFRPopupLayout(
        showCFR = true,
        properties = CFRPopupProperties(
            popupBodyColors = listOf(
                FirefoxTheme.colors.layerGradientEnd.toArgb(),
                FirefoxTheme.colors.layerGradientStart.toArgb(),
            ),
            dismissButtonColor = FirefoxTheme.colors.iconOnColor.toArgb(),
            indicatorDirection = CFRPopup.IndicatorDirection.DOWN,
            popupVerticalOffset = ARROW_VERTICAL_OFFSET.dp,
            indicatorArrowStartOffset = indicatorArrowStartOffset.dp,
        ),
        onCFRShown = {
            store.dispatch(MenuAction.ShowCFR)
        },
        onDismiss = {
            store.dispatch(MenuAction.DismissCFR)
        },
        title = {
            FirefoxTheme {
                Text(
                    text = stringResource(R.string.menu_cfr_title),
                    color = FirefoxTheme.colors.textOnColorPrimary,
                    style = FirefoxTheme.typography.subtitle2,
                )
            }
        },
        text = {
            FirefoxTheme {
                Text(
                    text = stringResource(R.string.menu_cfr_body),
                    color = FirefoxTheme.colors.textOnColorPrimary,
                    style = FirefoxTheme.typography.body2,
                )
            }
        },
    ) {
        MainMenu(
            accessPoint = accessPoint,
            store = store,
            syncStore = syncStore,
            installedAddons = installedAddons,
            showQuitMenu = showQuitMenu,
            isPrivate = isPrivate,
            isDesktopMode = isDesktopMode,
            isPdf = isPdf,
            isTranslationSupported = isTranslationSupported,
            isExtensionsProcessDisabled = isExtensionsProcessDisabled,
            onExtensionsMenuClick = { onExtensionsMenuClick() },
            onToolsMenuClick = { onToolsMenuClick() },
            onSaveMenuClick = { onSaveMenuClick() },
        )
    }
}

/**
 * Wrapper of [MainMenu]
 *
 * @param accessPoint The [MenuAccessPoint] that was used to navigate to the menu dialog.
 * @param store The [MenuStore] that is used for the current state.
 * @param syncStore The [SyncStore] used to determine account information.
 * @param installedAddons A list of installed [Addon]s to be shown.
 * @param showQuitMenu Whether or not to show the [QuitMenuGroup].
 * @param isPrivate Whether or not the browsing mode is in private mode.
 * @param isDesktopMode Whether or not the current site is in desktop mode.
 * @param isPdf Whether or not the current tab is a PDF.
 * @param isTranslationSupported Whether or not Translations are supported.
 * @param isExtensionsProcessDisabled Whether or not the extensions process is disabled due to extension errors.
 * @param onExtensionsMenuClick Invoked when the user clicks on extensions menu button.
 * @param onToolsMenuClick Invoked when the user clicks on tools menu button.
 * @param onSaveMenuClick Invoked when the user clicks on save menu button.
 */
@Suppress("LongMethod", "LongParameterList")
@Composable
internal fun MainMenu(
    accessPoint: MenuAccessPoint,
    store: MenuStore,
    syncStore: SyncStore,
    installedAddons: List<Addon>,
    showQuitMenu: Boolean,
    isPrivate: Boolean,
    isDesktopMode: Boolean,
    isPdf: Boolean,
    isTranslationSupported: Boolean,
    isExtensionsProcessDisabled: Boolean,
    onExtensionsMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
) {
    val account by syncStore.observeAsState(initialValue = null) { state -> state.account }
    val accountState by syncStore.observeAsState(initialValue = NotAuthenticated) { state ->
        state.accountState
    }

    MainMenu(
        accessPoint = accessPoint,
        account = account,
        accountState = accountState,
        installedAddons = installedAddons,
        isPrivate = isPrivate,
        isDesktopMode = isDesktopMode,
        isPdf = isPdf,
        isTranslationSupported = isTranslationSupported,
        showQuitMenu = showQuitMenu,
        isExtensionsProcessDisabled = isExtensionsProcessDisabled,
        onMozillaAccountButtonClick = {
            store.dispatch(
                MenuAction.Navigate.MozillaAccount(
                    accountState = accountState,
                    accesspoint = accessPoint,
                ),
            )
        },
        onHelpButtonClick = {
            store.dispatch(MenuAction.Navigate.Help)
        },
        onSettingsButtonClick = {
            store.dispatch(MenuAction.Navigate.Settings)
        },
        onNewTabMenuClick = {
            store.dispatch(MenuAction.Navigate.NewTab)
        },
        onNewPrivateTabMenuClick = {
            store.dispatch(MenuAction.Navigate.NewPrivateTab)
        },
        onSwitchToDesktopSiteMenuClick = {
            if (isDesktopMode) {
                store.dispatch(MenuAction.RequestMobileSite)
            } else {
                store.dispatch(MenuAction.RequestDesktopSite)
            }
        },
        onFindInPageMenuClick = {
            store.dispatch(MenuAction.FindInPage)
        },
        onToolsMenuClick = {
            store.dispatch(MenuAction.ToolsMenuClicked)
            onToolsMenuClick()
        },
        onSaveMenuClick = {
            store.dispatch(MenuAction.SaveMenuClicked)
            onSaveMenuClick()
        },
        onExtensionsMenuClick = {
            if (accessPoint == MenuAccessPoint.Home) {
                store.dispatch(MenuAction.Navigate.ManageExtensions)
            } else {
                onExtensionsMenuClick()
            }
        },
        onBookmarksMenuClick = {
            store.dispatch(MenuAction.Navigate.Bookmarks)
        },
        onHistoryMenuClick = {
            store.dispatch(MenuAction.Navigate.History)
        },
        onDownloadsMenuClick = {
            store.dispatch(MenuAction.Navigate.Downloads)
        },
        onPasswordsMenuClick = {
            store.dispatch(MenuAction.Navigate.Passwords)
        },
        onCustomizeHomepageMenuClick = {
            store.dispatch(MenuAction.Navigate.CustomizeHomepage)
        },
        onNewInFirefoxMenuClick = {
            store.dispatch(MenuAction.Navigate.ReleaseNotes)
        },
        onQuitMenuClick = {
            store.dispatch(MenuAction.DeleteBrowsingDataAndQuit)
        },
    )
}

/**
 * Wrapper column containing the main menu items.
 *
 * @param accessPoint The [MenuAccessPoint] that was used to navigate to the menu dialog.
 * @param account [Account] information available for a synced account.
 * @param accountState The [AccountState] of a Mozilla account.
 * @param installedAddons A list of installed [Addon]s to be shown.
 * @param isPrivate Whether or not the browsing mode is in private mode.
 * @param isDesktopMode Whether or not the desktop mode is enabled.
 * @param isPdf Whether or not the current tab is a PDF.
 * @param isTranslationSupported Whether or not translation is supported.
 * @param showQuitMenu Whether or not the button to delete browsing data and quit
 * should be visible.
 * @param isExtensionsProcessDisabled Whether or not the extensions process is disabled due to extension errors.
 * @param onMozillaAccountButtonClick Invoked when the user clicks on Mozilla account button.
 * @param onHelpButtonClick Invoked when the user clicks on the help button.
 * @param onSettingsButtonClick Invoked when the user clicks on the settings button.
 * @param onNewTabMenuClick Invoked when the user clicks on the new tab menu item.
 * @param onNewPrivateTabMenuClick Invoked when the user clicks on the new private tab menu item.
 * @param onSwitchToDesktopSiteMenuClick Invoked when the user clicks on the switch to desktop site
 * menu toggle.
 * @param onFindInPageMenuClick Invoked when the user clicks on the find in page menu item.
 * @param onToolsMenuClick Invoked when the user clicks on the tools menu item.
 * @param onSaveMenuClick Invoked when the user clicks on the save menu item.
 * @param onExtensionsMenuClick Invoked when the user clicks on the extensions menu item.
 * @param onBookmarksMenuClick Invoked when the user clicks on the bookmarks menu item.
 * @param onHistoryMenuClick Invoked when the user clicks on the history menu item.
 * @param onDownloadsMenuClick Invoked when the user clicks on the downloads menu item.
 * @param onPasswordsMenuClick Invoked when the user clicks on the passwords menu item.
 * @param onCustomizeHomepageMenuClick Invoked when the user clicks on the customize
 * homepage menu item.
 * @param onNewInFirefoxMenuClick Invoked when the user clicks on the release note menu item.
 * @param onQuitMenuClick Invoked when the user clicks on the quit menu item.
 */
@Suppress("LongParameterList")
@Composable
internal fun MainMenu(
    accessPoint: MenuAccessPoint,
    account: Account?,
    accountState: AccountState,
    installedAddons: List<Addon>,
    isPrivate: Boolean,
    isDesktopMode: Boolean,
    isPdf: Boolean,
    isTranslationSupported: Boolean,
    showQuitMenu: Boolean,
    isExtensionsProcessDisabled: Boolean,
    onMozillaAccountButtonClick: () -> Unit,
    onHelpButtonClick: () -> Unit,
    onSettingsButtonClick: () -> Unit,
    onNewTabMenuClick: () -> Unit,
    onNewPrivateTabMenuClick: () -> Unit,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
    onExtensionsMenuClick: () -> Unit,
    onBookmarksMenuClick: () -> Unit,
    onHistoryMenuClick: () -> Unit,
    onDownloadsMenuClick: () -> Unit,
    onPasswordsMenuClick: () -> Unit,
    onCustomizeHomepageMenuClick: () -> Unit,
    onNewInFirefoxMenuClick: () -> Unit,
    onQuitMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            MenuHeader(
                account = account,
                accountState = accountState,
                onMozillaAccountButtonClick = onMozillaAccountButtonClick,
                onHelpButtonClick = onHelpButtonClick,
                onSettingsButtonClick = onSettingsButtonClick,
            )
        },
    ) {
        NewTabsMenuGroup(
            accessPoint = accessPoint,
            isPrivate = isPrivate,
            onNewTabMenuClick = onNewTabMenuClick,
            onNewPrivateTabMenuClick = onNewPrivateTabMenuClick,
        )

        ToolsAndActionsMenuGroup(
            accessPoint = accessPoint,
            installedAddons = installedAddons,
            isDesktopMode = isDesktopMode,
            isPdf = isPdf,
            isTranslationSupported = isTranslationSupported,
            isExtensionsProcessDisabled = isExtensionsProcessDisabled,
            onSwitchToDesktopSiteMenuClick = onSwitchToDesktopSiteMenuClick,
            onFindInPageMenuClick = onFindInPageMenuClick,
            onToolsMenuClick = onToolsMenuClick,
            onSaveMenuClick = onSaveMenuClick,
            onExtensionsMenuClick = onExtensionsMenuClick,
        )

        LibraryMenuGroup(
            onBookmarksMenuClick = onBookmarksMenuClick,
            onHistoryMenuClick = onHistoryMenuClick,
            onDownloadsMenuClick = onDownloadsMenuClick,
            onPasswordsMenuClick = onPasswordsMenuClick,
        )

        if (accessPoint == MenuAccessPoint.Home) {
            HomepageMenuGroup(
                onCustomizeHomepageMenuClick = onCustomizeHomepageMenuClick,
                onNewInFirefoxMenuClick = onNewInFirefoxMenuClick,
            )
        }

        if (showQuitMenu) {
            QuitMenuGroup(
                onQuitMenuClick = onQuitMenuClick,
            )
        }
    }
}

@Composable
private fun QuitMenuGroup(
    onQuitMenuClick: () -> Unit,
) {
    MenuGroup {
        MenuItem(
            label = stringResource(
                id = R.string.browser_menu_delete_browsing_data_on_quit,
                stringResource(id = R.string.app_name),
            ),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_cross_circle_fill_24),
            state = MenuItemState.WARNING,
            onClick = onQuitMenuClick,
        )
    }
}

@Composable
private fun NewTabsMenuGroup(
    accessPoint: MenuAccessPoint,
    isPrivate: Boolean,
    onNewTabMenuClick: () -> Unit,
    onNewPrivateTabMenuClick: () -> Unit,
) {
    val isNewTabMenuEnabled: Boolean
    val isNewPrivateTabMenuEnabled: Boolean

    when (accessPoint) {
        MenuAccessPoint.Browser,
        MenuAccessPoint.External,
        -> {
            isNewTabMenuEnabled = true
            isNewPrivateTabMenuEnabled = true
        }

        MenuAccessPoint.Home -> {
            isNewTabMenuEnabled = isPrivate
            isNewPrivateTabMenuEnabled = !isPrivate
        }
    }

    MenuGroup {
        MenuItem(
            label = stringResource(id = R.string.library_new_tab),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_plus_24),
            state = if (isNewTabMenuEnabled) MenuItemState.ENABLED else MenuItemState.DISABLED,
            onClick = onNewTabMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(id = R.string.browser_menu_new_private_tab),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_private_mode_circle_fill_24),
            state = if (isNewPrivateTabMenuEnabled) MenuItemState.ENABLED else MenuItemState.DISABLED,
            onClick = onNewPrivateTabMenuClick,
        )
    }
}

@Suppress("LongParameterList", "LongMethod")
@Composable
private fun ToolsAndActionsMenuGroup(
    accessPoint: MenuAccessPoint,
    installedAddons: List<Addon>,
    isDesktopMode: Boolean,
    isPdf: Boolean,
    isTranslationSupported: Boolean,
    isExtensionsProcessDisabled: Boolean,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
    onExtensionsMenuClick: () -> Unit,
) {
    MenuGroup {
        if (accessPoint == MenuAccessPoint.Browser) {
            val labelId: Int
            val iconId: Int
            val menuItemState: MenuItemState

            if (isDesktopMode) {
                labelId = R.string.browser_menu_switch_to_mobile_site
                iconId = R.drawable.mozac_ic_device_mobile_24
                menuItemState = MenuItemState.ACTIVE
            } else {
                labelId = R.string.browser_menu_switch_to_desktop_site
                iconId = R.drawable.mozac_ic_device_desktop_24
                menuItemState = if (isPdf) MenuItemState.DISABLED else MenuItemState.ENABLED
            }

            MenuItem(
                label = stringResource(id = labelId),
                beforeIconPainter = painterResource(id = iconId),
                state = menuItemState,
                onClick = onSwitchToDesktopSiteMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_find_in_page_2),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_search_24),
                onClick = onFindInPageMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_tools),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_tool_24),
                description = stringResource(
                    id = if (isTranslationSupported) {
                        R.string.browser_menu_tools_description_with_translate
                    } else {
                        R.string.browser_menu_tools_description
                    },
                ),
                onClick = onToolsMenuClick,
                afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_save),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_save_24),
                description = stringResource(id = R.string.browser_menu_save_description),
                onClick = onSaveMenuClick,
                afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)
        }

        MenuItem(
            label = stringResource(id = R.string.browser_menu_extensions),
            description = if (isExtensionsProcessDisabled) {
                stringResource(R.string.browser_menu_extensions_disabled_description)
            } else {
                if (installedAddons.isEmpty()) {
                    stringResource(R.string.browser_menu_no_extensions_installed_description)
                } else {
                    var description: String? = ""
                    val context = LocalContext.current
                    for (addon in installedAddons) {
                        description += addon.displayName(context) + if (installedAddons.size > 1) ", " else ""
                    }
                    description
                }
            },
            descriptionState = if (isExtensionsProcessDisabled) {
                MenuItemState.WARNING
            } else {
                MenuItemState.ENABLED
            },
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_24),
            onClick = onExtensionsMenuClick,
            afterIconPainter = if (accessPoint != MenuAccessPoint.Home) {
                painterResource(id = R.drawable.mozac_ic_chevron_right_24)
            } else {
                null
            },
        )
    }
}

@Composable
private fun LibraryMenuGroup(
    onBookmarksMenuClick: () -> Unit,
    onHistoryMenuClick: () -> Unit,
    onDownloadsMenuClick: () -> Unit,
    onPasswordsMenuClick: () -> Unit,
) {
    MenuGroup {
        MenuItem(
            label = stringResource(id = R.string.library_bookmarks),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_bookmark_tray_fill_24),
            onClick = onBookmarksMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(id = R.string.library_history),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_history_24),
            onClick = onHistoryMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(id = R.string.library_downloads),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_download_24),
            onClick = onDownloadsMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(id = R.string.browser_menu_passwords),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_login_24),
            onClick = onPasswordsMenuClick,
        )
    }
}

@Composable
private fun HomepageMenuGroup(
    onCustomizeHomepageMenuClick: () -> Unit,
    onNewInFirefoxMenuClick: () -> Unit,
) {
    MenuGroup {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_customize_home_1),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_grid_add_24),
            onClick = onCustomizeHomepageMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(
                id = R.string.browser_menu_new_in_firefox,
                stringResource(id = R.string.app_name),
            ),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_whats_new_24),
            onClick = onNewInFirefoxMenuClick,
        )
    }
}

@LightDarkPreview
@Composable
private fun MenuDialogPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MainMenu(
                accessPoint = MenuAccessPoint.Browser,
                account = null,
                accountState = NotAuthenticated,
                installedAddons = emptyList(),
                isPrivate = false,
                isDesktopMode = false,
                isPdf = false,
                isTranslationSupported = true,
                showQuitMenu = true,
                isExtensionsProcessDisabled = true,
                onMozillaAccountButtonClick = {},
                onHelpButtonClick = {},
                onSettingsButtonClick = {},
                onNewTabMenuClick = {},
                onNewPrivateTabMenuClick = {},
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onToolsMenuClick = {},
                onSaveMenuClick = {},
                onExtensionsMenuClick = {},
                onBookmarksMenuClick = {},
                onHistoryMenuClick = {},
                onDownloadsMenuClick = {},
                onPasswordsMenuClick = {},
                onCustomizeHomepageMenuClick = {},
                onNewInFirefoxMenuClick = {},
                onQuitMenuClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun MenuDialogPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MainMenu(
                accessPoint = MenuAccessPoint.Home,
                account = null,
                accountState = NotAuthenticated,
                installedAddons = emptyList(),
                isPrivate = false,
                isDesktopMode = false,
                isPdf = false,
                isTranslationSupported = true,
                showQuitMenu = true,
                isExtensionsProcessDisabled = false,
                onMozillaAccountButtonClick = {},
                onHelpButtonClick = {},
                onSettingsButtonClick = {},
                onNewTabMenuClick = {},
                onNewPrivateTabMenuClick = {},
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onToolsMenuClick = {},
                onSaveMenuClick = {},
                onExtensionsMenuClick = {},
                onBookmarksMenuClick = {},
                onHistoryMenuClick = {},
                onDownloadsMenuClick = {},
                onPasswordsMenuClick = {},
                onCustomizeHomepageMenuClick = {},
                onNewInFirefoxMenuClick = {},
                onQuitMenuClick = {},
            )
        }
    }
}
