/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.displayName
import mozilla.components.feature.addons.ui.summary
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.AccountState.Authenticated
import mozilla.components.service.fxa.manager.AccountState.Authenticating
import mozilla.components.service.fxa.manager.AccountState.AuthenticationProblem
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.service.fxa.store.Account
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.menu.MenuDialogTestTag
import org.mozilla.fenix.components.menu.MenuDialogTestTag.EXTENSIONS
import org.mozilla.fenix.components.menu.compose.header.MenuNavHeader
import org.mozilla.fenix.components.menu.store.WebExtensionMenuItem
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import org.mozilla.fenix.utils.DURATION_MS_MAIN_MENU

/**
 * Wrapper column containing the main menu items.
 *
 * @param accessPoint The [MenuAccessPoint] that was used to navigate to the menu dialog.
 * @param account [Account] information available for a synced account.
 * @param accountState The [AccountState] of a Mozilla account.
 * @param showQuitMenu Whether or not the button to delete browsing data and quit
 * should be visible.
 * @param isSiteLoading Whether or not the tab is loading.
 * @param isExtensionsExpanded Whether or not the extensions menu is expanded.
 * @param isMoreMenuExpanded Whether or not the more menu is expanded.
 * @param isBookmarked Whether or not the current tab is bookmarked.
 * @param isDesktopMode Whether or not the desktop mode is enabled.
 * @param isPdf Whether or not the current tab is a PDF.
 * @param isReaderViewActive Whether or not Reader View is active or not.
 * @param isTranslationSupported Whether or not translation is supported.
 * @param isWebCompatReporterSupported Whether or not the report broken site feature is supported.
 * @param isExtensionsProcessDisabled Whether or not the extensions process is disabled due to extension errors.
 * @param allWebExtensionsDisabled Whether or not all web extensions are disabled.
 * @param extensionsMenuItemDescription The label of extensions menu item description.
 * @param scrollState The [ScrollState] used for vertical scrolling.
 * @param webExtensionMenuCount The number of web extensions.
 * @param onMoreMenuClick Invoked when the user clicks on the more menu item.
 * @param onCustomizeReaderViewMenuClick Invoked when the user clicks on the Customize Reader View button.
 * @param onMozillaAccountButtonClick Invoked when the user clicks on Mozilla account button.
 * @param onSettingsButtonClick Invoked when the user clicks on the settings button.
 * @param onBookmarkPageMenuClick Invoked when the user clicks on the bookmark page menu item.
 * @param onEditBookmarkButtonClick Invoked when the user clicks on the edit bookmark button.
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
 * @param onBackButtonClick Invoked when the user clicks on the back button.
 * @param onForwardButtonClick Invoked when the user clicks on the forward button.
 * @param onRefreshButtonClick Invoked when the user clicks on the refresh button.
 * @param onStopButtonClick Invoked when the user clicks on the stop button.
 * @param onShareButtonClick Invoked when the user clicks on the share button.
 * @param moreSettingsSubmenu The content of more menu item.
 * @param extensionSubmenu The content of extensions menu item to avoid configuration during animation.
 */
@Suppress("LongParameterList", "LongMethod")
@Composable
fun MainMenu(
    accessPoint: MenuAccessPoint,
    account: Account?,
    accountState: AccountState,
    showQuitMenu: Boolean,
    isSiteLoading: Boolean,
    isExtensionsExpanded: Boolean,
    isMoreMenuExpanded: Boolean,
    isBookmarked: Boolean,
    isDesktopMode: Boolean,
    isPdf: Boolean,
    isReaderViewActive: Boolean,
    isTranslationSupported: Boolean,
    isWebCompatReporterSupported: Boolean,
    isExtensionsProcessDisabled: Boolean,
    allWebExtensionsDisabled: Boolean,
    extensionsMenuItemDescription: String,
    scrollState: ScrollState,
    webExtensionMenuCount: Int,
    onMoreMenuClick: () -> Unit,
    onCustomizeReaderViewMenuClick: () -> Unit,
    onMozillaAccountButtonClick: () -> Unit,
    onSettingsButtonClick: () -> Unit,
    onBookmarkPageMenuClick: () -> Unit,
    onEditBookmarkButtonClick: () -> Unit,
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
    onBackButtonClick: (longPress: Boolean) -> Unit,
    onForwardButtonClick: (longPress: Boolean) -> Unit,
    onRefreshButtonClick: (longPress: Boolean) -> Unit,
    onStopButtonClick: () -> Unit,
    onShareButtonClick: () -> Unit,
    moreSettingsSubmenu: @Composable ColumnScope.() -> Unit,
    extensionSubmenu: @Composable ColumnScope.() -> Unit,
) {
    MenuFrame(
        header = {
            MenuNavHeader(
                state = if (accessPoint == MenuAccessPoint.Home) {
                    MenuItemState.DISABLED
                } else {
                    MenuItemState.ENABLED
                },
                isSiteLoading = if (accessPoint == MenuAccessPoint.Home) {
                    false
                } else {
                    isSiteLoading
                },
                onBackButtonClick = onBackButtonClick,
                onForwardButtonClick = onForwardButtonClick,
                onRefreshButtonClick = onRefreshButtonClick,
                onStopButtonClick = onStopButtonClick,
                onShareButtonClick = onShareButtonClick,
                isExtensionsExpanded = isExtensionsExpanded,
                isMoreMenuExpanded = isMoreMenuExpanded,
            )
        },
        scrollState = scrollState,
    ) {
        if (isReaderViewActive) {
            MenuGroup {
                MenuItem(
                    label = stringResource(id = R.string.browser_menu_customize_reader_view_2),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_tool_24),
                    onClick = onCustomizeReaderViewMenuClick,
                )
            }
        }

        if (accessPoint == MenuAccessPoint.Home) {
            HomepageMenuGroup(
                onCustomizeHomepageMenuClick = onCustomizeHomepageMenuClick,
                onNewInFirefoxMenuClick = onNewInFirefoxMenuClick,
                onExtensionsMenuClick = onExtensionsMenuClick,
                extensionsMenuItemDescription = extensionsMenuItemDescription,
                isExtensionsProcessDisabled = isExtensionsProcessDisabled,
                isExtensionsExpanded = isExtensionsExpanded,
                webExtensionMenuCount = webExtensionMenuCount,
                allWebExtensionsDisabled = allWebExtensionsDisabled,
                extensionSubmenu = extensionSubmenu,
            )
        }

        if (accessPoint == MenuAccessPoint.Browser) {
            ToolsAndActionsMenuGroup(
                isBookmarked = isBookmarked,
                isDesktopMode = isDesktopMode,
                isPdf = isPdf,
                isTranslationSupported = isTranslationSupported,
                isWebCompatReporterSupported = isWebCompatReporterSupported,
                extensionsMenuItemDescription = extensionsMenuItemDescription,
                isExtensionsProcessDisabled = isExtensionsProcessDisabled,
                isExtensionsExpanded = isExtensionsExpanded,
                moreMenuExpanded = isMoreMenuExpanded,
                webExtensionMenuCount = webExtensionMenuCount,
                allWebExtensionsDisabled = allWebExtensionsDisabled,
                onExtensionsMenuClick = onExtensionsMenuClick,
                onBookmarkPageMenuClick = onBookmarkPageMenuClick,
                onEditBookmarkButtonClick = onEditBookmarkButtonClick,
                onSwitchToDesktopSiteMenuClick = onSwitchToDesktopSiteMenuClick,
                onFindInPageMenuClick = onFindInPageMenuClick,
                onToolsMenuClick = onToolsMenuClick,
                onSaveMenuClick = onSaveMenuClick,
                onMoreMenuClick = onMoreMenuClick,
                moreSettingsSubmenu = moreSettingsSubmenu,
                extensionSubmenu = extensionSubmenu,
                )
        }

        LibraryMenuGroup(
            onBookmarksMenuClick = onBookmarksMenuClick,
            onHistoryMenuClick = onHistoryMenuClick,
            onDownloadsMenuClick = onDownloadsMenuClick,
            onPasswordsMenuClick = onPasswordsMenuClick,
        )

        MenuGroup {
            MozillaAccountMenuItem(
                account = account,
                accountState = accountState,
                onClick = onMozillaAccountButtonClick,
            )

            MenuItem(
                label = stringResource(id = R.string.browser_menu_settings),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_settings_24),
                onClick = onSettingsButtonClick,
            )
        }

        if (showQuitMenu) {
            QuitMenuGroup(
                onQuitMenuClick = onQuitMenuClick,
            )
        }
    }
}

@OptIn(ExperimentalComposeUiApi::class)
@Composable
private fun ExtensionsMenuItem(
    extensionsMenuItemDescription: String,
    isExtensionsProcessDisabled: Boolean,
    isExtensionsExpanded: Boolean,
    webExtensionMenuCount: Int,
    allWebExtensionsDisabled: Boolean,
    onExtensionsMenuClick: () -> Unit,
    extensionSubmenu: @Composable ColumnScope.() -> Unit,
) {
    Column {
        val leftPadding = if (webExtensionMenuCount > 0) 8.dp else 2.dp
        MenuItem(
            label = stringResource(id = R.string.browser_menu_extensions),
            description = extensionsMenuItemDescription,
            beforeIconPainter = if (isExtensionsProcessDisabled) {
                painterResource(id = R.drawable.mozac_ic_extension_disabled_24)
            } else {
                painterResource(id = R.drawable.mozac_ic_extension_24)
            },
            onClick = onExtensionsMenuClick,
            descriptionState = if (isExtensionsProcessDisabled) {
                MenuItemState.WARNING
            } else {
                MenuItemState.ENABLED
            },
            modifier = Modifier.semantics {
                testTag = EXTENSIONS
                testTagsAsResourceId = true
            },
        ) {
            if (isExtensionsProcessDisabled || allWebExtensionsDisabled) {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_settings_24),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )
                return@MenuItem
            }

            Row(
                modifier = Modifier
                    .background(
                        color = FirefoxTheme.colors.layerSearch,
                        shape = RoundedCornerShape(16.dp),
                    )
                    .padding(start = leftPadding, top = 2.dp, bottom = 2.dp, end = 2.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                if (webExtensionMenuCount > 0) {
                    Text(
                        text = webExtensionMenuCount.toString(),
                        color = FirefoxTheme.colors.textPrimary,
                        overflow = TextOverflow.Ellipsis,
                        style = FirefoxTheme.typography.caption,
                        maxLines = 1,
                    )
                }

                Icon(
                    painter = if (isExtensionsExpanded) {
                        painterResource(id = R.drawable.mozac_ic_chevron_up_20)
                    } else {
                        painterResource(id = R.drawable.mozac_ic_chevron_down_20)
                    },
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }

        MenuItemAnimation(
            isExpanded = isExtensionsExpanded,
            submenu = extensionSubmenu,
        )
    }
}

@Composable
private fun MenuItemAnimation(
    isExpanded: Boolean,
    submenu: @Composable ColumnScope.() -> Unit,
) {
    AnimatedVisibility(
        visible = isExpanded,
        enter = expandVertically(
            expandFrom = Alignment.Top,
            animationSpec = tween(
                durationMillis = DURATION_MS_MAIN_MENU,
                easing = LinearEasing,
            ),
        ) + fadeIn(
            animationSpec = tween(
                durationMillis = DURATION_MS_MAIN_MENU,
                easing = LinearEasing,
            ),
        ),
        exit = shrinkVertically(
            shrinkTowards = Alignment.Top,
            animationSpec = tween(
                durationMillis = DURATION_MS_MAIN_MENU,
                easing = LinearEasing,
            ),
        ) + fadeOut(
            animationSpec = tween(
                durationMillis = DURATION_MS_MAIN_MENU,
                easing = LinearEasing,
            ),
        ),
    ) {
        Column {
            submenu()
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

@Suppress("LongParameterList", "LongMethod")
@Composable
private fun ToolsAndActionsMenuGroup(
    isBookmarked: Boolean,
    isDesktopMode: Boolean,
    isPdf: Boolean,
    isTranslationSupported: Boolean,
    isWebCompatReporterSupported: Boolean,
    isExtensionsProcessDisabled: Boolean,
    extensionsMenuItemDescription: String,
    isExtensionsExpanded: Boolean,
    moreMenuExpanded: Boolean,
    webExtensionMenuCount: Int,
    allWebExtensionsDisabled: Boolean,
    onExtensionsMenuClick: () -> Unit,
    onBookmarkPageMenuClick: () -> Unit,
    onEditBookmarkButtonClick: () -> Unit,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
    onMoreMenuClick: () -> Unit,
    moreSettingsSubmenu: @Composable ColumnScope.() -> Unit,
    extensionSubmenu: @Composable ColumnScope.() -> Unit,
    ) {
    MenuGroup {
        val labelId = R.string.browser_menu_desktop_site
        val badgeText: String
        val iconId: Int
        val menuItemState: MenuItemState
        val badgeBackgroundColor: Color

        if (isDesktopMode) {
            badgeText = stringResource(id = R.string.browser_feature_desktop_site_on)
            badgeBackgroundColor = FirefoxTheme.colors.badgeActive
            iconId = R.drawable.mozac_ic_device_mobile_24
            menuItemState = MenuItemState.ACTIVE
        } else {
            badgeText = stringResource(id = R.string.browser_feature_desktop_site_off)
            badgeBackgroundColor = FirefoxTheme.colors.layerSearch
            iconId = R.drawable.mozac_ic_device_desktop_24
            menuItemState = if (isPdf) MenuItemState.DISABLED else MenuItemState.ENABLED
        }

        if (isBookmarked) {
                MenuItem(
                    label = stringResource(id = R.string.browser_menu_edit_bookmark),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_bookmark_fill_24),
                    state = MenuItemState.ACTIVE,
                    onClick = onEditBookmarkButtonClick,
                )
            } else {
                MenuItem(
                    label = stringResource(id = R.string.browser_menu_bookmark_this_page_2),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_bookmark_24),
                    onClick = onBookmarkPageMenuClick,
                )
            }

            MenuItem(
                label = stringResource(id = labelId),
                beforeIconPainter = painterResource(id = iconId),
                state = menuItemState,
                onClick = onSwitchToDesktopSiteMenuClick,
            ) {
            if (menuItemState == MenuItemState.DISABLED) {
                return@MenuItem
            }

            Badge(
                badgeText = badgeText,
                state = menuItemState,
                badgeBackgroundColor = badgeBackgroundColor,
            )
        }

        MenuItem(
            label = stringResource(id = R.string.browser_menu_find_in_page_2),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_search_24),
            onClick = onFindInPageMenuClick,
        )

        MenuItem(
            label = stringResource(id = R.string.browser_menu_tools),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_tool_24),
            description = when {
                isTranslationSupported && isWebCompatReporterSupported -> stringResource(
                    R.string.browser_menu_tools_description_with_translate_with_report_site_2,
                )
                isTranslationSupported -> stringResource(
                    R.string.browser_menu_tools_description_with_translate_without_report_site,
                )
                isWebCompatReporterSupported -> stringResource(
                    R.string.browser_menu_tools_description_with_report_site_2,
                )
                else -> stringResource(
                    R.string.browser_menu_tools_description_without_report_site,
                )
            },
            onClick = onToolsMenuClick,
            modifier = Modifier.testTag(MenuDialogTestTag.TOOLS),
            afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
        )

        MenuItem(
            label = stringResource(id = R.string.browser_menu_save),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_save_24),
            description = stringResource(id = R.string.browser_menu_save_description),
            onClick = onSaveMenuClick,
            modifier = Modifier.testTag(MenuDialogTestTag.SAVE),
            afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
        )

        ExtensionsMenuItem(
            extensionsMenuItemDescription = extensionsMenuItemDescription,
            isExtensionsProcessDisabled = isExtensionsProcessDisabled,
            isExtensionsExpanded = isExtensionsExpanded,
            webExtensionMenuCount = webExtensionMenuCount,
            allWebExtensionsDisabled = allWebExtensionsDisabled,
            onExtensionsMenuClick = onExtensionsMenuClick,
            extensionSubmenu = extensionSubmenu,
        )

        MoreMenuButtonGroup(
            moreMenuExpanded = moreMenuExpanded,
            onMoreMenuClick = onMoreMenuClick,
        )

        MenuItemAnimation(
            isExpanded = moreMenuExpanded,
            submenu = moreSettingsSubmenu,
        )
    }
}

@Composable
private fun MoreMenuButtonGroup(
    moreMenuExpanded: Boolean,
    onMoreMenuClick: () -> Unit,
) {
    MenuItem(
        label = if (moreMenuExpanded) {
            stringResource(id = R.string.browser_menu_less_settings)
        } else {
            stringResource(id = R.string.browser_menu_more_settings)
        },
        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_ellipsis_horizontal_24),
        onClick = onMoreMenuClick,
    ) {
        Row(
            modifier = Modifier
                .background(
                    color = FirefoxTheme.colors.layerSearch,
                    shape = RoundedCornerShape(16.dp),
                )
                .padding(2.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                painter = if (moreMenuExpanded) {
                    painterResource(id = R.drawable.mozac_ic_chevron_up_20)
                } else {
                    painterResource(id = R.drawable.mozac_ic_chevron_down_20)
                },
                contentDescription = null,
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }
    }
}

@Composable
private fun LibraryMenuGroup(
    onBookmarksMenuClick: () -> Unit,
    onHistoryMenuClick: () -> Unit,
    onDownloadsMenuClick: () -> Unit,
    onPasswordsMenuClick: () -> Unit,
) {
    val spacerWidth = 2.dp
    val innerRounding = 4.dp
    val outerRounding = 28.dp

    val leftShape = RoundedCornerShape(
        topStart = outerRounding, topEnd = innerRounding,
        bottomStart = outerRounding, bottomEnd = innerRounding,
    )
    val middleShape = RoundedCornerShape(innerRounding)
    val rightShape = RoundedCornerShape(
        topStart = innerRounding,
        topEnd = outerRounding, bottomStart = innerRounding, bottomEnd = outerRounding,
    )

    Row(
        Modifier
            .fillMaxWidth()
            .height(IntrinsicSize.Min),
        horizontalArrangement = Arrangement.SpaceEvenly,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        LibraryMenuItem(
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight(),
            iconRes = R.drawable.mozac_ic_history_24,
            labelRes = R.string.library_history,
            shape = leftShape,
            onClick = onHistoryMenuClick,
        )

        Spacer(Modifier.width(spacerWidth))

        LibraryMenuItem(
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight(),
            iconRes = R.drawable.mozac_ic_bookmark_tray_fill_24,
            labelRes = R.string.library_bookmarks,
            shape = middleShape,
            onClick = onBookmarksMenuClick,
        )

        Spacer(Modifier.width(spacerWidth))

        LibraryMenuItem(
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight(),
            iconRes = R.drawable.mozac_ic_download_24,
            labelRes = R.string.library_downloads,
            shape = middleShape,
            onClick = onDownloadsMenuClick,
        )

        Spacer(Modifier.width(spacerWidth))

        LibraryMenuItem(
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight(),
            iconRes = R.drawable.mozac_ic_login_24,
            labelRes = R.string.browser_menu_passwords,
            shape = rightShape,
            onClick = onPasswordsMenuClick,
        )
    }
}

@Suppress("LongParameterList")
@Composable
private fun HomepageMenuGroup(
    extensionsMenuItemDescription: String,
    isExtensionsProcessDisabled: Boolean,
    isExtensionsExpanded: Boolean,
    webExtensionMenuCount: Int,
    allWebExtensionsDisabled: Boolean,
    onExtensionsMenuClick: () -> Unit,
    onCustomizeHomepageMenuClick: () -> Unit,
    onNewInFirefoxMenuClick: () -> Unit,
    extensionSubmenu: @Composable ColumnScope.() -> Unit,
) {
    MenuGroup {
        MenuItem(
            label = stringResource(
                id = R.string.browser_menu_new_in_firefox,
                stringResource(id = R.string.app_name),
            ),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_whats_new_24),
            onClick = onNewInFirefoxMenuClick,
        )

        MenuItem(
            label = stringResource(id = R.string.browser_menu_customize_home_1),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_grid_add_24),
            onClick = onCustomizeHomepageMenuClick,
        )

        ExtensionsMenuItem(
            extensionsMenuItemDescription = extensionsMenuItemDescription,
            isExtensionsProcessDisabled = isExtensionsProcessDisabled,
            isExtensionsExpanded = isExtensionsExpanded,
            webExtensionMenuCount = webExtensionMenuCount,
            allWebExtensionsDisabled = allWebExtensionsDisabled,
            onExtensionsMenuClick = onExtensionsMenuClick,
            extensionSubmenu = extensionSubmenu,
        )
    }
}

@Composable
internal fun MozillaAccountMenuItem(
    account: Account?,
    accountState: AccountState,
    onClick: () -> Unit,
) {
    val label: String
    val description: String?

    when (accountState) {
        NotAuthenticated -> {
            label = stringResource(id = R.string.browser_menu_sign_in)
            description = stringResource(id = R.string.browser_menu_sign_in_caption)
        }

        AuthenticationProblem -> {
            label = stringResource(id = R.string.browser_menu_sign_back_in_to_sync)
            description = stringResource(id = R.string.browser_menu_syncing_paused_caption)
        }

        Authenticated -> {
            label = account?.displayName ?: account?.email
                ?: stringResource(id = R.string.browser_menu_account_settings)
            description = null
        }

        is Authenticating -> {
            label = ""
            description = null
        }
    }

    MenuItem(
        label = label,
        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_avatar_circle_24),
        description = description,
        descriptionState = if (accountState is AuthenticationProblem) {
            MenuItemState.WARNING
        } else {
            MenuItemState.ENABLED
        },
        afterIconPainter = if (accountState is AuthenticationProblem) {
            painterResource(R.drawable.mozac_ic_warning_fill_24)
        } else {
            null
        },
        onClick = onClick,
    )
}

@Suppress("LongParameterList")
@Composable
internal fun Addons(
    accessPoint: MenuAccessPoint,
    availableAddons: List<Addon>,
    webExtensionMenuItems: List<WebExtensionMenuItem>,
    addonInstallationInProgress: Addon?,
    recommendedAddons: List<Addon>,
    onAddonSettingsClick: (Addon) -> Unit,
    onAddonClick: (Addon) -> Unit,
    onInstallAddonClick: (Addon) -> Unit,
    onManageExtensionsMenuClick: () -> Unit,
    onDiscoverMoreExtensionsMenuClick: () -> Unit,
    onWebExtensionMenuItemClick: () -> Unit,
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        if (accessPoint == MenuAccessPoint.Home && availableAddons.isNotEmpty()) {
            AddonsMenuItems(
                availableAddons = availableAddons,
                iconPainter = painterResource(id = R.drawable.mozac_ic_settings_24),
                onClick = {
                    onWebExtensionMenuItemClick()
                    onAddonClick(it)
                },
                onIconClick = { onAddonSettingsClick(it) },
            )
        } else if (accessPoint == MenuAccessPoint.Browser && webExtensionMenuItems.isNotEmpty()) {
            WebExtensionMenuItems(
                webExtensionMenuItems = webExtensionMenuItems,
                onWebExtensionMenuItemClick = onWebExtensionMenuItemClick,
                availableAddons = availableAddons,
                onSettingsClick = { onAddonSettingsClick(it) },
            )
        } else if (recommendedAddons.isNotEmpty()) {
            AddonsMenuItems(
                availableAddons = recommendedAddons,
                addonInstallationInProgress = addonInstallationInProgress,
                onClick = {
                    onWebExtensionMenuItemClick()
                    onAddonClick(it)
                },
                onIconClick = { onInstallAddonClick(it) },
            )
        }

        if (availableAddons.isNotEmpty()) {
            MoreExtensionsMenuItem(
                onManageExtensionsMenuClick,
                stringResource(id = R.string.browser_menu_manage_extensions),
            )
        } else {
            MoreExtensionsMenuItem(
                onDiscoverMoreExtensionsMenuClick,
                stringResource(id = R.string.browser_menu_discover_more_extensions),
            )
        }
    }
}

@Composable
private fun AddonsMenuItems(
    availableAddons: List<Addon>,
    addonInstallationInProgress: Addon? = null,
    iconPainter: Painter? = null,
    onClick: (Addon) -> Unit,
    onIconClick: (Addon) -> Unit,
) {
    Column(
        modifier = Modifier.padding(top = 2.dp),
        verticalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        for (addon in availableAddons) {
            val description = stringResource(
                R.string.browser_menu_extension_plus_icon_content_description_2,
                addon.displayName(LocalContext.current),
            )

            AddonMenuItem(
                addon = addon,
                addonInstallationInProgress = addonInstallationInProgress,
                iconPainter = iconPainter ?: painterResource(id = R.drawable.mozac_ic_plus_24),
                iconDescription = if (iconPainter != null) addon.summary(LocalContext.current) else description,
                showDivider = true,
                onClick = { onClick(addon) },
                onIconClick = { onIconClick(addon) },
            )
        }
    }
}

@Composable
private fun WebExtensionMenuItems(
    webExtensionMenuItems: List<WebExtensionMenuItem>,
    onWebExtensionMenuItemClick: () -> Unit,
    availableAddons: List<Addon> = emptyList(),
    onSettingsClick: (Addon) -> Unit,
) {
    Column(
        modifier = Modifier.padding(top = 2.dp),
        verticalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        for (webExtensionMenuItem in webExtensionMenuItems) {
            val addon = availableAddons.find { it.id == webExtensionMenuItem.id }

            WebExtensionMenuItem(
                label = webExtensionMenuItem.label,
                iconPainter = webExtensionMenuItem.icon?.let { icon ->
                    BitmapPainter(image = icon.asImageBitmap())
                }
                    ?: painterResource(R.drawable.mozac_ic_web_extension_default_icon),
                enabled = webExtensionMenuItem.enabled,
                badgeText = webExtensionMenuItem.badgeText,
                onClick = {
                    onWebExtensionMenuItemClick()
                    webExtensionMenuItem.onClick()
                },
                onSettingsClick = {
                    if (addon != null) {
                        onSettingsClick(addon)
                    }
                },
            )
        }
    }
}

@Composable
private fun MoreExtensionsMenuItem(
    onClick: () -> Unit,
    label: String,
) {
    Column(
        modifier = Modifier
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = LocalIndication.current,
            ) { onClick.invoke() }
            .clearAndSetSemantics {
                role = Role.Button
            }
            .wrapContentSize()
            .clip(shape = RoundedCornerShape(4.dp))
            .background(
                color = FirefoxTheme.colors.layer3,
            ),
    ) {
        MenuTextItem(
            label = label,
            iconPainter = painterResource(id = R.drawable.mozac_ic_external_link_24),
            modifier = Modifier.padding(start = 40.dp),
        )
    }
}

@PreviewLightDark
@Composable
private fun MenuDialogPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1),
        ) {
            MainMenu(
                accessPoint = MenuAccessPoint.Browser,
                account = null,
                accountState = NotAuthenticated,
                showQuitMenu = true,
                isSiteLoading = false,
                isExtensionsExpanded = false,
                isMoreMenuExpanded = true,
                isBookmarked = false,
                isDesktopMode = false,
                isPdf = false,
                isReaderViewActive = false,
                isTranslationSupported = true,
                isWebCompatReporterSupported = true,
                isExtensionsProcessDisabled = true,
                allWebExtensionsDisabled = false,
                extensionsMenuItemDescription = "No extensions enabled",
                scrollState = ScrollState(0),
                webExtensionMenuCount = 1,
                onMoreMenuClick = {},
                onCustomizeReaderViewMenuClick = {},
                onMozillaAccountButtonClick = {},
                onSettingsButtonClick = {},
                onBookmarkPageMenuClick = {},
                onEditBookmarkButtonClick = {},
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
                onBackButtonClick = {},
                onForwardButtonClick = {},
                onRefreshButtonClick = {},
                onStopButtonClick = {},
                onShareButtonClick = {},
                moreSettingsSubmenu = {},
                extensionSubmenu = {},
            )
        }
    }
}

@Suppress("LongMethod")
@Preview
@Composable
private fun MenuDialogPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1),
        ) {
            MainMenu(
                accessPoint = MenuAccessPoint.Home,
                account = null,
                accountState = NotAuthenticated,
                showQuitMenu = true,
                isSiteLoading = false,
                isExtensionsExpanded = true,
                isMoreMenuExpanded = true,
                isBookmarked = false,
                isDesktopMode = false,
                isPdf = false,
                isReaderViewActive = false,
                isTranslationSupported = true,
                isWebCompatReporterSupported = true,
                isExtensionsProcessDisabled = false,
                allWebExtensionsDisabled = false,
                extensionsMenuItemDescription = "No extensions enabled",
                scrollState = ScrollState(0),
                webExtensionMenuCount = 0,
                onMoreMenuClick = {},
                onCustomizeReaderViewMenuClick = {},
                onMozillaAccountButtonClick = {},
                onSettingsButtonClick = {},
                onBookmarkPageMenuClick = {},
                onEditBookmarkButtonClick = {},
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
                onBackButtonClick = {},
                onForwardButtonClick = {},
                onRefreshButtonClick = {},
                onStopButtonClick = {},
                onShareButtonClick = {},
                moreSettingsSubmenu = {},
                extensionSubmenu = {
                    Addons(
                        accessPoint = MenuAccessPoint.Home,
                        availableAddons = listOf(),
                        webExtensionMenuItems = listOf(),
                        addonInstallationInProgress = null,
                        recommendedAddons = listOf(
                            Addon(
                                id = "id",
                                translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                                translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                            ),
                            Addon(
                                id = "id",
                                translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                                translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                            ),
                        ),
                        onAddonSettingsClick = {},
                        onAddonClick = {},
                        onInstallAddonClick = {},
                        onManageExtensionsMenuClick = {},
                        onDiscoverMoreExtensionsMenuClick = {},
                        onWebExtensionMenuItemClick = {},
                    )
                },
            )
        }
    }
}
