/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ui.banner

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.menu.DropdownMenu
import mozilla.components.compose.base.menu.MenuItem
import mozilla.components.ui.tabcounter.TabCounter
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Banner
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.TabsTrayTestTag
import org.mozilla.fenix.tabstray.ext.getMenuItems
import org.mozilla.fenix.tabstray.ui.tabstray.TabsTray
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.max

private val ICON_SIZE = 24.dp
private const val MAX_WIDTH_TAB_ROW_PERCENT = 0.5f
private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f
private const val TAB_COUNT_SHOW_CFR = 6

/**
 * Top-level UI for displaying the banner in [TabsTray].
 *
 * @param selectedPage The current page the Tabs Tray is on.
 * @param normalTabCount The total number of open normal tabs.
 * @param privateTabCount The total number of open private tabs.
 * @param syncedTabCount The total number of open synced tabs.
 * @param selectionMode [TabsTrayState.Mode] indicating the current selection mode (e.g., normal, multi-select).
 * @param isInDebugMode True if the debug variant is active or the secret menu is enabled for the session.
 * @param shouldShowTabAutoCloseBanner Whether the tab auto-close banner should be displayed.
 * @param shouldShowLockPbmBanner Whether the lock private browsing mode banner should be displayed.
 * @param onTabPageIndicatorClicked Invoked when the user clicks on a tab page indicator.
 * @param onSaveToCollectionClick Invoked when the user clicks the "Save to Collection" button in multi-select mode.
 * @param onShareSelectedTabsClick Invoked when the user clicks the "Share" button in multi-select mode.
 * @param onShareAllTabsClick Invoked when the user clicks the "Share All Tabs" menu item.
 * @param onTabSettingsClick Invoked when the user clicks the "Tab Settings" menu item.
 * @param onRecentlyClosedClick Invoked when the user clicks the "Recently Closed Tabs" menu item.
 * @param onAccountSettingsClick Invoked when the user clicks the "Account Settings" menu item.
 * @param onDeleteAllTabsClick Invoked when the user clicks the "Close All Tabs" menu item.
 * @param onDeleteSelectedTabsClick Invoked when the user clicks the "Close Selected Tabs" menu item.
 * @param onBookmarkSelectedTabsClick Invoked when the user clicks the "Bookmark Selected Tabs" menu item.
 * @param onForceSelectedTabsAsInactiveClick Invoked when the user clicks the "Mark Tabs as Inactive" menu item.
 * @param onDismissClick Invoked when accessibility services or UI automation request a dismissal.
 * @param onTabAutoCloseBannerViewOptionsClick Invoked when the user clicks to view auto-close settings from the banner.
 * @param onTabsTrayPbmLockedClick Invoked when the user interacts with the lock private browsing mode banner.
 * @param onTabsTrayPbmLockedDismiss Invoked when the user clicks on either button in the
 * lock private browsing mode banner.
 * @param onTabAutoCloseBannerDismiss Invoked when the user dismisses the auto-close banner.
 * @param onTabAutoCloseBannerShown Invoked when the auto-close banner is shown to the user.
 * @param onEnterMultiselectModeClick Invoked when the user enters multi-select mode.
 * @param onExitSelectModeClick Invoked when the user exits multi-select mode.
 */
@Suppress("LongParameterList", "LongMethod")
@Composable
fun TabsTrayBanner(
    selectedPage: Page,
    normalTabCount: Int,
    privateTabCount: Int,
    syncedTabCount: Int,
    selectionMode: TabsTrayState.Mode,
    isInDebugMode: Boolean,
    shouldShowTabAutoCloseBanner: Boolean,
    shouldShowLockPbmBanner: Boolean,
    onTabPageIndicatorClicked: (Page) -> Unit,
    onSaveToCollectionClick: () -> Unit,
    onShareSelectedTabsClick: () -> Unit,
    onShareAllTabsClick: () -> Unit,
    onTabSettingsClick: () -> Unit,
    onRecentlyClosedClick: () -> Unit,
    onAccountSettingsClick: () -> Unit,
    onDeleteAllTabsClick: () -> Unit,
    onDeleteSelectedTabsClick: () -> Unit,
    onBookmarkSelectedTabsClick: () -> Unit,
    onForceSelectedTabsAsInactiveClick: () -> Unit,
    onDismissClick: () -> Unit,
    onTabAutoCloseBannerViewOptionsClick: () -> Unit,
    onTabsTrayPbmLockedClick: () -> Unit,
    onTabsTrayPbmLockedDismiss: () -> Unit,
    onTabAutoCloseBannerDismiss: () -> Unit,
    onTabAutoCloseBannerShown: () -> Unit,
    onEnterMultiselectModeClick: () -> Unit,
    onExitSelectModeClick: () -> Unit,
) {
    val isInMultiSelectMode by remember(selectionMode) {
        derivedStateOf {
            selectionMode is TabsTrayState.Mode.Select
        }
    }
    val showTabAutoCloseBanner by remember(
        shouldShowTabAutoCloseBanner,
        normalTabCount,
        privateTabCount,
    ) {
        derivedStateOf {
            shouldShowTabAutoCloseBanner && max(
                normalTabCount,
                privateTabCount,
            ) >= TAB_COUNT_SHOW_CFR
        }
    }

    var hasAcknowledgedAutoCloseBanner by remember { mutableStateOf(false) }
    var hasAcknowledgedPbmLockBanner by remember { mutableStateOf(false) }

    val menuItems = selectionMode.getMenuItems(
        shouldShowInactiveButton = isInDebugMode,
        onBookmarkSelectedTabsClick = onBookmarkSelectedTabsClick,
        onCloseSelectedTabsClick = onDeleteSelectedTabsClick,
        onMakeSelectedTabsInactive = onForceSelectedTabsAsInactiveClick,
        selectedPage = selectedPage,
        normalTabCount = normalTabCount,
        privateTabCount = privateTabCount,
        onTabSettingsClick = onTabSettingsClick,
        onRecentlyClosedClick = onRecentlyClosedClick,
        onEnterMultiselectModeClick = onEnterMultiselectModeClick,
        onShareAllTabsClick = onShareAllTabsClick,
        onDeleteAllTabsClick = onDeleteAllTabsClick,
        onAccountSettingsClick = onAccountSettingsClick,
    )

    Column(
        modifier = Modifier.testTag(tag = TabsTrayTestTag.BANNER_ROOT),
    ) {
        if (isInMultiSelectMode) {
            MultiSelectBanner(
                menuItems = menuItems,
                selectedTabCount = selectionMode.selectedTabs.size,
                onExitSelectModeClick = onExitSelectModeClick,
                onSaveToCollectionsClick = onSaveToCollectionClick,
                onShareSelectedTabs = onShareSelectedTabsClick,
            )
        } else {
            TabPageBanner(
                menuItems = menuItems,
                selectedPage = selectedPage,
                normalTabCount = normalTabCount,
                privateTabCount = privateTabCount,
                syncedTabCount = syncedTabCount,
                onTabPageIndicatorClicked = onTabPageIndicatorClicked,
                onDismissClick = onDismissClick,
            )
        }

        when {
            !hasAcknowledgedAutoCloseBanner && showTabAutoCloseBanner -> {
                onTabAutoCloseBannerShown()

                Divider()

                Banner(
                    message = stringResource(id = R.string.tab_tray_close_tabs_banner_message),
                    button1Text = stringResource(id = R.string.tab_tray_close_tabs_banner_negative_button_text),
                    button2Text = stringResource(id = R.string.tab_tray_close_tabs_banner_positive_button_text),
                    onButton1Click = {
                        hasAcknowledgedAutoCloseBanner = true
                        onTabAutoCloseBannerDismiss()
                    },
                    onButton2Click = {
                        hasAcknowledgedAutoCloseBanner = true
                        onTabAutoCloseBannerViewOptionsClick()
                    },
                )
            }

            !hasAcknowledgedPbmLockBanner && shouldShowLockPbmBanner -> {
                // After this bug: https://bugzilla.mozilla.org/show_bug.cgi?id=1965545
                // is resolved, we should swap the button 1 and button 2 click actions.
                Banner(
                    message = stringResource(id = R.string.private_tab_cfr_title),
                    button1Text = stringResource(id = R.string.private_tab_cfr_negative),
                    button2Text = stringResource(id = R.string.private_tab_cfr_positive),
                    onButton1Click = {
                        hasAcknowledgedPbmLockBanner = true
                        onTabsTrayPbmLockedDismiss()
                    },
                    onButton2Click = {
                        hasAcknowledgedPbmLockBanner = true
                        onTabsTrayPbmLockedClick()
                        onTabsTrayPbmLockedDismiss()
                    },
                )
            }
        }
    }
}

@Suppress("LongMethod")
@Composable
private fun TabPageBanner(
    menuItems: List<MenuItem>,
    selectedPage: Page,
    normalTabCount: Int,
    privateTabCount: Int,
    syncedTabCount: Int,
    onTabPageIndicatorClicked: (Page) -> Unit,
    onDismissClick: () -> Unit,
) {
    val selectedColor = FirefoxTheme.colors.iconActive
    val inactiveColor = FirefoxTheme.colors.iconPrimaryInactive
    var showMenu by remember { mutableStateOf(false) }

    Column(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
        Spacer(modifier = Modifier.height(dimensionResource(id = R.dimen.bottom_sheet_handle_top_margin)))

        BottomSheetHandle(
            onRequestDismiss = onDismissClick,
            contentDescription = stringResource(R.string.a11y_action_label_collapse),
            modifier = Modifier
                .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                .align(Alignment.CenterHorizontally)
                .testTag(TabsTrayTestTag.BANNER_HANDLE),
        )

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(80.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            TabRow(
                selectedTabIndex = selectedPage.ordinal,
                modifier = Modifier
                    .fillMaxWidth(MAX_WIDTH_TAB_ROW_PERCENT)
                    .fillMaxHeight(),
                containerColor = Color.Transparent,
                contentColor = selectedColor,
                divider = {},
            ) {
                Tab(
                    selected = selectedPage == Page.NormalTabs,
                    onClick = { onTabPageIndicatorClicked(Page.NormalTabs) },
                    modifier = Modifier
                        .fillMaxHeight()
                        .testTag(TabsTrayTestTag.NORMAL_TABS_PAGE_BUTTON),
                    selectedContentColor = selectedColor,
                    unselectedContentColor = inactiveColor,
                ) {
                    TabCounter(
                        tabCount = normalTabCount,
                        textColor = LocalContentColor.current,
                        iconColor = LocalContentColor.current,
                    )
                }

                Tab(
                    selected = selectedPage == Page.PrivateTabs,
                    onClick = { onTabPageIndicatorClicked(Page.PrivateTabs) },
                    modifier = Modifier
                        .fillMaxHeight()
                        .testTag(TabsTrayTestTag.PRIVATE_TABS_PAGE_BUTTON),
                    icon = {
                        Icon(
                            painter = painterResource(id = R.drawable.ic_private_browsing),
                            contentDescription = stringResource(
                                id = R.string.tabs_header_private_tabs_counter_title,
                                privateTabCount.toString(),
                            ),
                        )
                    },
                    selectedContentColor = selectedColor,
                    unselectedContentColor = inactiveColor,
                )

                Tab(
                    selected = selectedPage == Page.SyncedTabs,
                    onClick = { onTabPageIndicatorClicked(Page.SyncedTabs) },
                    modifier = Modifier
                        .fillMaxHeight()
                        .testTag(TabsTrayTestTag.SYNCED_TABS_PAGE_BUTTON),
                    icon = {
                        Icon(
                            painter = painterResource(id = R.drawable.ic_synced_tabs),
                            contentDescription = stringResource(
                                id = R.string.tabs_header_synced_tabs_counter_title,
                                syncedTabCount.toString(),
                            ),
                        )
                    },
                    selectedContentColor = selectedColor,
                    unselectedContentColor = inactiveColor,
                )
            }

            Spacer(modifier = Modifier.weight(1.0f))

            IconButton(
                onClick = { showMenu = true },
                modifier = Modifier
                    .align(Alignment.CenterVertically)
                    .testTag(TabsTrayTestTag.THREE_DOT_BUTTON),
            ) {
                DropdownMenu(
                    menuItems = menuItems,
                    expanded = showMenu,
                    offset = DpOffset(x = 0.dp, y = -ICON_SIZE),
                    onDismissRequest = {
                        showMenu = false
                    },
                )
                Icon(
                    painter = painterResource(R.drawable.ic_menu),
                    contentDescription = stringResource(id = R.string.open_tabs_menu),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }
    }
}

/**
 * Banner displayed in multi select mode.
 *
 * @param menuItems List of items in the menu.
 * @param selectedTabCount Number of selected tabs.
 * @param onExitSelectModeClick Invoked when the user clicks on exit select mode button.
 * @param onSaveToCollectionsClick Invoked when the user clicks on the save to collection button.
 * @param onShareSelectedTabs Invoked when the user clicks on the share button.
 */
@Suppress("LongMethod")
@Composable
private fun MultiSelectBanner(
    menuItems: List<MenuItem>,
    selectedTabCount: Int,
    onExitSelectModeClick: () -> Unit,
    onSaveToCollectionsClick: () -> Unit,
    onShareSelectedTabs: () -> Unit,
) {
    var showMenu by remember { mutableStateOf(false) }
    val buttonsEnabled by remember(selectedTabCount) {
        derivedStateOf {
            selectedTabCount > 0
        }
    }
    val buttonTint = if (buttonsEnabled) {
        FirefoxTheme.colors.iconOnColor
    } else {
        FirefoxTheme.colors.iconOnColorDisabled
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(88.dp)
            .background(color = FirefoxTheme.colors.layerAccent),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        IconButton(onClick = onExitSelectModeClick) {
            Icon(
                painter = painterResource(id = R.drawable.ic_close),
                contentDescription = stringResource(id = R.string.tab_tray_close_multiselect_content_description),
                tint = FirefoxTheme.colors.iconOnColor,
            )
        }

        Text(
            text = stringResource(R.string.tab_tray_multi_select_title, selectedTabCount),
            modifier = Modifier.testTag(TabsTrayTestTag.SELECTION_COUNTER),
            style = FirefoxTheme.typography.headline6,
            color = FirefoxTheme.colors.textOnColorPrimary,
        )

        Spacer(modifier = Modifier.weight(1.0f))

        IconButton(
            onClick = onSaveToCollectionsClick,
            modifier = Modifier.testTag(TabsTrayTestTag.COLLECTIONS_BUTTON),
            enabled = buttonsEnabled,
        ) {
            Icon(
                painter = painterResource(id = R.drawable.ic_tab_collection),
                contentDescription = stringResource(
                    id = R.string.tab_tray_collection_button_multiselect_content_description,
                ),
                tint = buttonTint,
            )
        }

        IconButton(
            onClick = onShareSelectedTabs,
            enabled = buttonsEnabled,
        ) {
            Icon(
                painter = painterResource(id = R.drawable.ic_share),
                contentDescription = stringResource(
                    id = R.string.tab_tray_multiselect_share_content_description,
                ),
                tint = buttonTint,
            )
        }

        IconButton(
            onClick = { showMenu = true },
            enabled = buttonsEnabled,
        ) {
            Icon(
                painter = painterResource(id = R.drawable.ic_menu),
                contentDescription = stringResource(id = R.string.tab_tray_multiselect_menu_content_description),
                tint = buttonTint,
            )

            DropdownMenu(
                menuItems = menuItems,
                expanded = showMenu,
                offset = DpOffset(x = 0.dp, y = -ICON_SIZE),
                onDismissRequest = { showMenu = false },
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun TabsTrayBannerPreview() {
    TabsTrayBannerPreviewRoot(
        selectedPage = Page.PrivateTabs,
        normalTabCount = 5,
    )
}

@PreviewLightDark
@Composable
private fun TabsTrayBannerInfinityPreview() {
    TabsTrayBannerPreviewRoot(
        normalTabCount = 200,
    )
}

@PreviewLightDark
@Composable
private fun TabsTrayBannerAutoClosePreview() {
    TabsTrayBannerPreviewRoot(
        shouldShowTabAutoCloseBanner = true,
    )
}

@PreviewLightDark
@Composable
private fun TabsTrayBannerMultiselectPreview() {
    TabsTrayBannerPreviewRoot(
        selectMode = TabsTrayState.Mode.Select(
            setOf(
                TabSessionState(
                    id = "1",
                    content = ContentState(
                        url = "www.mozilla.com",
                    ),
                ),
                TabSessionState(
                    id = "2",
                    content = ContentState(
                        url = "www.mozilla.com",
                    ),
                ),
            ),
        ),
    )
}

@PreviewLightDark
@Composable
private fun TabsTrayBannerMultiselectNoTabsSelectedPreview() {
    TabsTrayBannerPreviewRoot(
        selectMode = TabsTrayState.Mode.Select(selectedTabs = setOf()),
    )
}

@Composable
private fun TabsTrayBannerPreviewRoot(
    selectMode: TabsTrayState.Mode = TabsTrayState.Mode.Normal,
    selectedPage: Page = Page.NormalTabs,
    normalTabCount: Int = 10,
    privateTabCount: Int = 10,
    syncedTabCount: Int = 10,
    shouldShowTabAutoCloseBanner: Boolean = false,
    shouldShowLockPbmBanner: Boolean = false,
) {
    val normalTabs = generateFakeTabsList(normalTabCount)
    val privateTabs = generateFakeTabsList(privateTabCount)

    val tabsTrayStore = remember {
        TabsTrayStore(
            initialState = TabsTrayState(
                selectedPage = selectedPage,
                mode = selectMode,
                normalTabs = normalTabs,
                privateTabs = privateTabs,
            ),
        )
    }

    FirefoxTheme {
        Box(modifier = Modifier.size(400.dp)) {
            TabsTrayBanner(
                selectedPage = selectedPage,
                normalTabCount = normalTabCount,
                privateTabCount = privateTabCount,
                syncedTabCount = syncedTabCount,
                selectionMode = selectMode,
                isInDebugMode = true,
                shouldShowTabAutoCloseBanner = shouldShowTabAutoCloseBanner,
                shouldShowLockPbmBanner = shouldShowLockPbmBanner,
                onTabPageIndicatorClicked = { page ->
                    tabsTrayStore.dispatch(TabsTrayAction.PageSelected(page))
                },
                onSaveToCollectionClick = {},
                onShareSelectedTabsClick = {},
                onShareAllTabsClick = {},
                onTabSettingsClick = {},
                onRecentlyClosedClick = {},
                onAccountSettingsClick = {},
                onDeleteAllTabsClick = {},
                onBookmarkSelectedTabsClick = {},
                onDeleteSelectedTabsClick = {},
                onForceSelectedTabsAsInactiveClick = {},
                onDismissClick = {},
                onTabAutoCloseBannerViewOptionsClick = {},
                onTabsTrayPbmLockedClick = {},
                onTabsTrayPbmLockedDismiss = {},
                onTabAutoCloseBannerDismiss = {},
                onTabAutoCloseBannerShown = {},
                onEnterMultiselectModeClick = {
                    tabsTrayStore.dispatch(TabsTrayAction.EnterSelectMode)
                },
                onExitSelectModeClick = {
                    tabsTrayStore.dispatch(TabsTrayAction.ExitSelectMode)
                },
            )
        }
    }
}

private fun generateFakeTabsList(
    tabCount: Int = 10,
    isPrivate: Boolean = false,
): List<TabSessionState> =
    List(tabCount) { index ->
        TabSessionState(
            id = "tabId$index-$isPrivate",
            content = ContentState(
                url = "www.mozilla.com",
                private = isPrivate,
            ),
        )
    }
