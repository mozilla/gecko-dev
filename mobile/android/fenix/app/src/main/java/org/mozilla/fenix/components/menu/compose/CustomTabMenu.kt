/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import android.app.PendingIntent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.compose.base.annotation.LightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Wrapper column containing the main menu items.
 *
 * @param isPdf Whether or not the current custom tab is a PDF.
 * @param isDesktopMode Whether or not the current site is in desktop mode.
 * @param isSandboxCustomTab Whether or not the current custom tab is sandboxed.
 * @param customTabMenuItems Additional [CustomTabMenuItem]s to be displayed to the custom tab menu.
 * @param onCustomMenuItemClick Invoked when the user clicks on [CustomTabMenuItem]s.
 * @param onSwitchToDesktopSiteMenuClick Invoked when the user clicks on the switch to desktop site
 * menu toggle.
 * @param onFindInPageMenuClick Invoked when the user clicks on the find in page menu item.
 * @param onOpenInFirefoxMenuClick Invoked when the user clicks on the open in browser menu item.
 * @param onShareMenuClick Invoked when the user clicks on the share menu item.
 */
@Suppress("LongParameterList")
@Composable
internal fun CustomTabMenu(
    isPdf: Boolean,
    isDesktopMode: Boolean,
    isSandboxCustomTab: Boolean,
    customTabMenuItems: List<CustomTabMenuItem>?,
    onCustomMenuItemClick: (PendingIntent) -> Unit,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onOpenInFirefoxMenuClick: () -> Unit,
    onShareMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {},
    ) {
        MenuGroup {
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
                menuItemState = MenuItemState.ENABLED
            }

            MenuItem(
                label = stringResource(id = labelId),
                beforeIconPainter = painterResource(id = iconId),
                state = if (isPdf) MenuItemState.DISABLED else menuItemState,
                onClick = onSwitchToDesktopSiteMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_find_in_page_2),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_search_24),
                onClick = onFindInPageMenuClick,
            )
        }

        MenuGroup {
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

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_share_2),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_share_android_24),
                onClick = onShareMenuClick,
            )
        }

        if (!customTabMenuItems.isNullOrEmpty()) {
            MenuGroup {
                customTabMenuItems.forEachIndexed { index, customTabMenuItem ->
                    if (index > 0) {
                        Divider(color = FirefoxTheme.colors.borderSecondary)
                    }

                    MenuTextItem(
                        label = customTabMenuItem.name,
                        onClick = { onCustomMenuItemClick(customTabMenuItem.pendingIntent) },
                    )
                }
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun CustomTabMenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            CustomTabMenu(
                isPdf = false,
                isDesktopMode = false,
                isSandboxCustomTab = false,
                customTabMenuItems = null,
                onCustomMenuItemClick = { _: PendingIntent -> },
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onOpenInFirefoxMenuClick = {},
                onShareMenuClick = {},
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
                isPdf = true,
                isDesktopMode = false,
                isSandboxCustomTab = false,
                customTabMenuItems = null,
                onCustomMenuItemClick = { _: PendingIntent -> },
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onOpenInFirefoxMenuClick = {},
                onShareMenuClick = {},
            )
        }
    }
}
