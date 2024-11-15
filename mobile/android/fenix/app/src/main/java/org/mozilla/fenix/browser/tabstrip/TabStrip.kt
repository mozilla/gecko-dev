/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import android.graphics.Bitmap
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyListItemInfo
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.systemGestureExclusion
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.traversalIndex
import androidx.compose.ui.tooling.preview.Devices
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.coerceIn
import androidx.compose.ui.unit.dp
import androidx.core.text.BidiFormatter
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.compose.HorizontalFadingEdgeBox
import org.mozilla.fenix.compose.ext.thenConditional
import org.mozilla.fenix.tabstray.browser.compose.DragItemContainer
import org.mozilla.fenix.tabstray.browser.compose.createListReorderState
import org.mozilla.fenix.tabstray.browser.compose.detectListPressAndDrag
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

private val minTabStripItemWidth = 130.dp
private val maxTabStripItemWidth = 280.dp
private val tabItemHeight = 40.dp
private val tabStripIconSize = 24.dp
private val spaceBetweenTabs = 4.dp
private val tabStripListContentStartPadding = 8.dp
private val titleFadeWidth = 16.dp
private val tabStripHorizontalPadding = 16.dp

/**
 * Top level composable for the tabs strip.
 *
 * @param onHome Whether or not the tabs strip is in the home screen.
 * @param browserStore The [BrowserStore] instance used to observe tabs state.
 * @param appStore The [AppStore] instance used to observe browsing mode.
 * @param tabsUseCases The [TabsUseCases] instance to perform tab actions.
 * @param onAddTabClick Invoked when the add tab button is clicked.
 * @param onCloseTabClick Invoked when a tab is closed.
 * @param onLastTabClose Invoked when the last remaining open tab is closed.
 * @param onSelectedTabClick Invoked when a tab is selected.
 * @param onPrivateModeToggleClick Invoked when the private mode toggle button is clicked.
 * @param onTabCounterClick Invoked when tab counter is clicked.
 */
@Composable
fun TabStrip(
    onHome: Boolean = false,
    browserStore: BrowserStore = components.core.store,
    appStore: AppStore = components.appStore,
    tabsUseCases: TabsUseCases = components.useCases.tabsUseCases,
    onAddTabClick: () -> Unit,
    onCloseTabClick: (isPrivate: Boolean) -> Unit,
    onLastTabClose: (isPrivate: Boolean) -> Unit,
    onSelectedTabClick: () -> Unit,
    onPrivateModeToggleClick: (mode: BrowsingMode) -> Unit,
    onTabCounterClick: () -> Unit,
) {
    val isPossiblyPrivateMode by appStore.observeAsState(false) { it.mode.isPrivate }
    val state by browserStore.observeAsState(TabStripState.initial) {
        it.toTabStripState(
            isSelectDisabled = onHome,
            isPossiblyPrivateMode = isPossiblyPrivateMode,
            addTab = onAddTabClick,
            toggleBrowsingMode = { isPrivate ->
                toggleBrowsingMode(isPrivate, onPrivateModeToggleClick, appStore)
            },
            closeTab = { isPrivate, numberOfTabs ->
                it.selectedTabId?.let { selectedTabId ->
                    closeTab(
                        numberOfTabs = numberOfTabs,
                        isPrivate = isPrivate,
                        tabsUseCases = tabsUseCases,
                        tabId = selectedTabId,
                        onLastTabClose = onLastTabClose,
                        onCloseTabClick = onCloseTabClick,
                    )
                }
            },
        )
    }

    TabStripContent(
        state = state,
        onAddTabClick = onAddTabClick,
        onPrivateModeToggleClick = {
            toggleBrowsingMode(state.isPrivateMode, onPrivateModeToggleClick, appStore)
        },
        onCloseTabClick = { tabId, isPrivate ->
            closeTab(
                numberOfTabs = state.tabs.size,
                isPrivate = isPrivate,
                tabsUseCases = tabsUseCases,
                tabId = tabId,
                onLastTabClose = onLastTabClose,
                onCloseTabClick = onCloseTabClick,
            )
        },
        onSelectedTabClick = {
            tabsUseCases.selectTab(it)
            onSelectedTabClick()
        },
        onMove = { tabId, targetId, placeAfter ->
            if (tabId != targetId) {
                tabsUseCases.moveTabs(listOf(tabId), targetId, placeAfter)
            }
        },
        onTabCounterClick = onTabCounterClick,
    )
}

@Composable
private fun TabStripContent(
    state: TabStripState,
    onAddTabClick: () -> Unit,
    onPrivateModeToggleClick: () -> Unit,
    onCloseTabClick: (id: String, isPrivate: Boolean) -> Unit,
    onSelectedTabClick: (id: String) -> Unit,
    onMove: (tabId: String, targetId: String, placeAfter: Boolean) -> Unit,
    onTabCounterClick: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer3)
            .systemGestureExclusion()
            .padding(horizontal = tabStripHorizontalPadding),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Row(
            modifier = Modifier.weight(1f, fill = false),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            IconButton(
                onClick = onPrivateModeToggleClick,
            ) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_private_mode_24),
                    tint = FirefoxTheme.colors.iconPrimary,
                    contentDescription = if (state.isPrivateMode) {
                        stringResource(R.string.content_description_disable_private_browsing_button)
                    } else {
                        stringResource(R.string.content_description_private_browsing_button)
                    },
                )
            }

            TabsList(
                state = state,
                modifier = Modifier.weight(1f, fill = false),
                onCloseTabClick = onCloseTabClick,
                onSelectedTabClick = onSelectedTabClick,
                onMove = onMove,
            )

            IconButton(onClick = onAddTabClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_plus_24),
                    tint = FirefoxTheme.colors.iconPrimary,
                    contentDescription = stringResource(R.string.add_tab),
                )
            }
        }

        TabStripTabCounterButton(
            tabCount = state.tabs.size,
            size = dimensionResource(R.dimen.tab_strip_height),
            menuItems = state.menuItems,
            privacyBadgeVisible = state.isPrivateMode,
            onClick = onTabCounterClick,
        )
    }
}

@Composable
@OptIn(ExperimentalFoundationApi::class)
private fun TabsList(
    state: TabStripState,
    modifier: Modifier = Modifier,
    onCloseTabClick: (id: String, isPrivate: Boolean) -> Unit,
    onSelectedTabClick: (id: String) -> Unit,
    onMove: (tabId: String, targetId: String, placeAfter: Boolean) -> Unit,
) {
    BoxWithConstraints(modifier = modifier) {
        val listState = rememberLazyListState()
        // Calculate the width of each tab item based on available width and the number of tabs and
        // taking into account the space between tabs.
        val availableWidth = maxWidth - tabStripListContentStartPadding
        val tabWidth = (availableWidth / state.tabs.size) - spaceBetweenTabs

        val reorderState = createListReorderState(
            listState = listState,
            onMove = { movedTab, adjacentTab ->
                onMove(
                    (movedTab.key as String),
                    (adjacentTab.key as String),
                    movedTab.index < adjacentTab.index,
                )
            },
            ignoredItems = emptyList(),
        )

        LazyRow(
            modifier = Modifier
                .detectListPressAndDrag(
                    reorderState = reorderState,
                    listState = listState,
                    shouldLongPressToDrag = true,
                )
                .selectableGroup(),
            state = listState,
            contentPadding = PaddingValues(start = tabStripListContentStartPadding),
        ) {
            itemsIndexed(
                items = state.tabs,
                key = { _, item -> item.id },
            ) { index, itemState ->
                DragItemContainer(
                    state = reorderState,
                    key = itemState.id,
                    position = index,
                ) {
                    TabItem(
                        state = itemState,
                        onCloseTabClick = onCloseTabClick,
                        onSelectedTabClick = onSelectedTabClick,
                        modifier = Modifier
                            .padding(end = spaceBetweenTabs)
                            .animateItem()
                            .width(
                                tabWidth.coerceIn(
                                    minimumValue = minTabStripItemWidth,
                                    maximumValue = maxTabStripItemWidth,
                                ),
                            )
                            .thenConditional(
                                modifier = Modifier.semantics { traversalIndex = -1f },
                                predicate = { itemState.isSelected },
                            ),
                    )
                }
            }
        }

        if (state.tabs.isNotEmpty()) {
            // When a new tab is added, scroll to the end of the list. This is done here instead of
            // in onCloseTabClick so this acts on state change which can occur from any other
            // place e.g. tabs tray.
            LaunchedEffect(state.tabs.last().id) {
                listState.scrollToItem(state.tabs.size)
            }

            // When a tab is selected, scroll to the selected tab. This is done here instead of
            // in onSelectedTabClick so this acts on state change which can occur from any other
            // place e.g. tabs tray.
            val selectedTab = state.tabs.firstOrNull { it.isSelected }
            LaunchedEffect(selectedTab?.id) {
                if (selectedTab != null) {
                    val selectedItemInfo =
                        listState.layoutInfo.visibleItemsInfo.firstOrNull { it.key == selectedTab.id }

                    if (listState.isItemPartiallyVisible(selectedItemInfo) || selectedItemInfo == null) {
                        listState.animateScrollToItem(state.tabs.indexOf(selectedTab))
                    }
                }
            }
        }
    }
}

private fun LazyListState.isItemPartiallyVisible(itemInfo: LazyListItemInfo?) =
    itemInfo != null &&
        (itemInfo.offset + itemInfo.size > layoutInfo.viewportEndOffset || itemInfo.offset < 0)

@Composable
@Suppress("LongMethod")
private fun TabItem(
    state: TabStripItem,
    modifier: Modifier = Modifier,
    onCloseTabClick: (id: String, isPrivate: Boolean) -> Unit,
    onSelectedTabClick: (id: String) -> Unit,
) {
    val backgroundColor = if (state.isSelected) {
        FirefoxTheme.colors.tabActive
    } else {
        FirefoxTheme.colors.tabInactive
    }
    val closeTabLabel = stringResource(R.string.close_tab)

    TabStripCard(
        modifier = modifier.height(tabItemHeight),
        backgroundColor = backgroundColor,
        elevation = if (state.isSelected) {
            selectedTabStripCardElevation
        } else {
            defaultTabStripCardElevation
        },
    ) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .clickable { onSelectedTabClick(state.id) }
                .semantics {
                    role = Role.Tab
                    selected = state.isSelected
                    customActions = listOf(
                        CustomAccessibilityAction(
                            label = closeTabLabel,
                        ) {
                            onCloseTabClick(state.id, state.isPrivate)
                            true
                        },
                    )
                },
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Row(
                modifier = Modifier.weight(1f),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                // This makes sure that isRtl is only calculated when the title changes.
                val isTitleRtl = remember(state.title) {
                    BidiFormatter.getInstance().isRtl(state.title)
                }

                Spacer(modifier = Modifier.size(8.dp))

                TabStripIcon(
                    url = state.url,
                    icon = state.icon,
                )

                Spacer(modifier = Modifier.size(8.dp))

                HorizontalFadingEdgeBox(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxHeight(),
                    fadeWidth = titleFadeWidth,
                    backgroundColor = backgroundColor,
                    isContentRtl = isTitleRtl,
                ) {
                    Text(
                        text = state.title,
                        modifier = Modifier.align(Alignment.CenterStart),
                        color = FirefoxTheme.colors.textPrimary,
                        softWrap = false,
                        maxLines = 1,
                        style = FirefoxTheme.typography.subtitle2,
                    )
                }
            }

            if (state.isCloseButtonVisible) {
                IconButton(
                    onClick = { onCloseTabClick(state.id, state.isPrivate) },
                    modifier = if (state.isSelected) {
                        Modifier.semantics {}
                    } else {
                        Modifier.clearAndSetSemantics {}
                    },
                ) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_cross_20),
                        tint = if (state.isSelected) {
                            FirefoxTheme.colors.iconPrimary
                        } else {
                            FirefoxTheme.colors.iconSecondary
                        },
                        contentDescription = stringResource(
                            id = R.string.close_tab_title,
                            state.title,
                        ),
                    )
                }
            } else {
                Spacer(modifier = Modifier.size(8.dp))
            }
        }
    }
}

@Composable
private fun TabStripIcon(
    url: String,
    icon: Bitmap?,
) {
    Box(
        modifier = Modifier
            .size(tabStripIconSize)
            .clip(CircleShape),
        contentAlignment = Alignment.Center,
    ) {
        if (icon != null && !icon.isRecycled) {
            Image(
                bitmap = icon.asImageBitmap(),
                contentDescription = null,
                modifier = Modifier
                    .size(tabStripIconSize)
                    .clip(CircleShape),
            )
        } else {
            Favicon(
                url = url,
                size = tabStripIconSize,
            )
        }
    }
}

private fun closeTab(
    numberOfTabs: Int,
    isPrivate: Boolean,
    tabsUseCases: TabsUseCases,
    tabId: String,
    onLastTabClose: (isPrivate: Boolean) -> Unit,
    onCloseTabClick: (isPrivate: Boolean) -> Unit,
) {
    if (numberOfTabs == 1) {
        onLastTabClose(isPrivate)
    }
    tabsUseCases.removeTab(tabId)
    onCloseTabClick(isPrivate)
}

/**
 * Invoking the callback is required so the caller can update the browsing mode in cases where
 * appStore.dispatch(AppAction.ModeChange(newMode)) is not enough. This bug is tracked here:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1923650
 */
private fun toggleBrowsingMode(
    isCurrentModePrivate: Boolean,
    onPrivateModeToggleClick: (mode: BrowsingMode) -> Unit,
    appStore: AppStore,
) {
    val newMode = BrowsingMode.fromBoolean(!isCurrentModePrivate)
    onPrivateModeToggleClick(newMode)
    appStore.dispatch(AppAction.ModeChange(newMode))
}

private class TabUIStateParameterProvider : PreviewParameterProvider<TabStripState> {
    override val values: Sequence<TabStripState>
        get() = sequenceOf(
            TabStripState(
                listOf(
                    TabStripItem(
                        id = "1",
                        title = "Tab 1",
                        url = "https://www.mozilla.org",
                        isPrivate = false,
                        isSelected = false,
                    ),
                    TabStripItem(
                        id = "2",
                        title = "Tab 2 with a very long title that should be truncated",
                        url = "https://www.mozilla.org",
                        isPrivate = false,
                        isSelected = false,
                    ),
                    TabStripItem(
                        id = "3",
                        title = "Selected tab",
                        url = "https://www.mozilla.org",
                        isPrivate = false,
                        isSelected = true,
                    ),
                    TabStripItem(
                        id = "p1",
                        title = "Private tab 1",
                        url = "https://www.mozilla.org",
                        isPrivate = true,
                        isSelected = false,
                    ),
                    TabStripItem(
                        id = "p2",
                        title = "Private selected tab",
                        url = "https://www.mozilla.org",
                        isPrivate = true,
                        isSelected = true,
                    ),
                ),
                isPrivateMode = false,
                tabCounterMenuItems = emptyList(),
            ),
        )
}

@Preview(device = Devices.PIXEL_TABLET)
@Composable
private fun TabStripPreview(
    @PreviewParameter(TabUIStateParameterProvider::class) tabStripState: TabStripState,
) {
    FirefoxTheme {
        TabStripContentPreview(tabStripState.tabs.filter { !it.isPrivate })
    }
}

@Preview(device = Devices.PIXEL_TABLET)
@Composable
private fun TabStripPreviewDarkMode(
    @PreviewParameter(TabUIStateParameterProvider::class) tabStripState: TabStripState,
) {
    FirefoxTheme(theme = Theme.Dark) {
        TabStripContentPreview(tabStripState.tabs.filter { !it.isPrivate })
    }
}

@Preview(device = Devices.PIXEL_TABLET)
@Composable
private fun TabStripPreviewPrivateMode(
    @PreviewParameter(TabUIStateParameterProvider::class) tabStripState: TabStripState,
) {
    FirefoxTheme(theme = Theme.Private) {
        TabStripContentPreview(tabStripState.tabs.filter { it.isPrivate })
    }
}

@Composable
private fun TabStripContentPreview(tabs: List<TabStripItem>) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(dimensionResource(id = R.dimen.tab_strip_height)),
        contentAlignment = Alignment.Center,
    ) {
        TabStripContent(
            state = TabStripState(
                tabs = tabs,
                isPrivateMode = false,
                tabCounterMenuItems = emptyList(),
            ),
            onAddTabClick = {},
            onPrivateModeToggleClick = {},
            onCloseTabClick = { _, _ -> },
            onSelectedTabClick = {},
            onMove = { _, _, _ -> },
            onTabCounterClick = {},
        )
    }
}

@Preview(device = Devices.PIXEL_TABLET)
@Composable
private fun TabStripPreview() {
    val browserStore = BrowserStore()

    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(dimensionResource(id = R.dimen.tab_strip_height)),
            contentAlignment = Alignment.Center,
        ) {
            TabStrip(
                appStore = AppStore(),
                browserStore = browserStore,
                tabsUseCases = TabsUseCases(browserStore),
                onAddTabClick = {
                    val tab = createTab(
                        url = "www.example.com",
                    )
                    browserStore.dispatch(TabListAction.AddTabAction(tab))
                },
                onLastTabClose = {},
                onCloseTabClick = {},
                onSelectedTabClick = {},
                onPrivateModeToggleClick = {},
                onTabCounterClick = {},
            )
        }
    }
}
