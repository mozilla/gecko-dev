/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import android.app.PendingIntent
import androidx.compose.foundation.Image
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.CustomTabMenuItem
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.header.MenuNavHeader
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Wrapper column containing the main menu items.
 *
 * @param isSiteLoading Whether or not the custom tab is currently loading.
 * @param isPdf Whether or not the current custom tab is a PDF.
 * @param isDesktopMode Whether or not the current site is in desktop mode.
 * @param isSandboxCustomTab Whether or not the current custom tab is sandboxed.
 * @param customTabMenuItems Additional [CustomTabMenuItem]s to be displayed to the custom tab menu.
 * @param onCustomMenuItemClick Invoked when the user clicks on [CustomTabMenuItem]s.
 * @param scrollState The [ScrollState] used for vertical scrolling.
 * @param onSwitchToDesktopSiteMenuClick Invoked when the user clicks on the switch to desktop site
 * menu toggle.
 * @param onFindInPageMenuClick Invoked when the user clicks on the find in page menu item.
 * @param onOpenInFirefoxMenuClick Invoked when the user clicks on the open in browser menu item.
 * @param onBackButtonClick Invoked when the user clicks on the back button.
 * @param onForwardButtonClick Invoked when the user clicks on the forward button.
 * @param onRefreshButtonClick Invoked when the user clicks on the refresh button.
 * @param onStopButtonClick Invoked when the user clicks on the stop button.
 * @param onShareButtonClick Invoked when the user clicks on the share button.
 */
@Suppress("LongParameterList", "LongMethod")
@Composable
internal fun CustomTabMenu(
    isSiteLoading: Boolean,
    isPdf: Boolean,
    isDesktopMode: Boolean,
    isSandboxCustomTab: Boolean,
    customTabMenuItems: List<CustomTabMenuItem>?,
    onCustomMenuItemClick: (PendingIntent) -> Unit,
    scrollState: ScrollState,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onOpenInFirefoxMenuClick: () -> Unit,
    onBackButtonClick: (longPress: Boolean) -> Unit,
    onForwardButtonClick: (longPress: Boolean) -> Unit,
    onRefreshButtonClick: (longPress: Boolean) -> Unit,
    onStopButtonClick: () -> Unit,
    onShareButtonClick: () -> Unit,
) {
    MenuFrame(
        header = {
            MenuNavHeader(
                isSiteLoading = isSiteLoading,
                onBackButtonClick = onBackButtonClick,
                onForwardButtonClick = onForwardButtonClick,
                onRefreshButtonClick = onRefreshButtonClick,
                onStopButtonClick = onStopButtonClick,
                onShareButtonClick = onShareButtonClick,
                isExtensionsExpanded = false,
                isMoreMenuExpanded = false,
            )
        },
        scrollState = scrollState,
    ) {
        MenuGroup {
            val badgeText: String
            val menuItemState: MenuItemState
            val badgeBackgroundColor: Color

            if (isDesktopMode) {
                badgeText = stringResource(id = R.string.browser_feature_desktop_site_on)
                badgeBackgroundColor = FirefoxTheme.colors.badgeActive
                menuItemState = MenuItemState.ACTIVE
            } else {
                badgeText = stringResource(id = R.string.browser_feature_desktop_site_off)
                badgeBackgroundColor = FirefoxTheme.colors.layerSearch
                menuItemState = if (isPdf) MenuItemState.DISABLED else MenuItemState.ENABLED
            }

            MenuItem(
                label = stringResource(
                    id = R.string.browser_menu_open_in_fenix,
                    stringResource(id = R.string.app_name),
                ),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_open_in),
                onClick = onOpenInFirefoxMenuClick,
                state = if (isSandboxCustomTab) {
                    MenuItemState.DISABLED
                } else {
                    MenuItemState.ENABLED
                },
            )

            MenuItem(
                label = stringResource(id = R.string.browser_menu_find_in_page),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_search_24),
                onClick = onFindInPageMenuClick,
            )

            MenuItem(
                label = stringResource(id = R.string.browser_menu_desktop_site),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_device_mobile_24),
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
        }

        if (!customTabMenuItems.isNullOrEmpty()) {
            MenuGroup {
                customTabMenuItems.forEach { customTabMenuItem ->
                    MenuTextItem(
                        label = customTabMenuItem.name,
                        onClick = { onCustomMenuItemClick(customTabMenuItem.pendingIntent) },
                    )
                }
            }
        }

        PoweredByFirefoxItem()
    }
}

/**
 * A menu item that shows the "Powered by Firefox" text and logo.
 *
 * @param modifier [Modifier] to be applied to the layout.
 */
@Composable
private fun PoweredByFirefoxItem(modifier: Modifier = Modifier) {
    Row(
        horizontalArrangement = Arrangement.Center,
        modifier = modifier.fillMaxWidth(),
    ) {
        Image(
            painter = painterResource(id = R.drawable.ic_firefox),
            contentDescription = null,
            modifier = Modifier
                .size(16.dp)
                .align(Alignment.CenterVertically),
        )

        Spacer(Modifier.width(4.dp))

        Text(
            text = stringResource(
                id = R.string.browser_menu_powered_by2,
                stringResource(id = R.string.app_name),
            ),
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.caption,
        )
    }
}

@PreviewLightDark
@Composable
private fun CustomTabMenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            CustomTabMenu(
                isSiteLoading = true,
                isPdf = false,
                isDesktopMode = false,
                isSandboxCustomTab = false,
                customTabMenuItems = null,
                onCustomMenuItemClick = { _: PendingIntent -> },
                scrollState = rememberScrollState(),
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onOpenInFirefoxMenuClick = {},
                onBackButtonClick = {},
                onForwardButtonClick = {},
                onRefreshButtonClick = {},
                onStopButtonClick = {},
                onShareButtonClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun CustomTabMenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            CustomTabMenu(
                isSiteLoading = false,
                isPdf = true,
                isDesktopMode = false,
                isSandboxCustomTab = false,
                customTabMenuItems = null,
                onCustomMenuItemClick = { _: PendingIntent -> },
                scrollState = rememberScrollState(),
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onOpenInFirefoxMenuClick = {},
                onBackButtonClick = {},
                onForwardButtonClick = {},
                onRefreshButtonClick = {},
                onStopButtonClick = {},
                onShareButtonClick = {},
            )
        }
    }
}
