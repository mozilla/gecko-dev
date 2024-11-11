/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertEquals
import org.junit.Test

class TabStripStateTest {

    @Test
    fun `WHEN browser state tabs is empty THEN tabs strip state tabs is empty`() {
        val browserState = BrowserState(tabs = emptyList())
        val actual = browserState.toTabStripState(
            isSelectDisabled = false,
            isPossiblyPrivateMode = false,
            addTab = {},
            toggleBrowsingMode = {},
            closeTab = { _, _ -> },
        )

        val expected = TabStripState(
            tabs = emptyList(),
            isPrivateMode = false,
            tabCounterMenuItems = allMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `WHEN private mode is off THEN tabs strip state tabs should include only non private tabs`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Example 2",
                    private = true,
                    id = "2",
                ),
                createTab(
                    url = "https://example3.com",
                    title = "Example 3",
                    private = false,
                    id = "3",
                ),
            ),
            selectedTabId = "1",
        )
        val actual =
            browserState.toTabStripState(
                isSelectDisabled = false,
                isPossiblyPrivateMode = false,
                addTab = {},
                toggleBrowsingMode = {},
                closeTab = { _, _ -> },
            )

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = true,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "3",
                    title = "Example 3",
                    url = "https://example3.com",
                    isSelected = false,
                    isPrivate = false,
                ),
            ),
            isPrivateMode = false,
            tabCounterMenuItems = allMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `WHEN private mode is possibly on THEN tabs strip state tabs should include only private tabs`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Private Example",
                    private = true,
                    id = "2",
                ),
                createTab(
                    url = "https://example3.com",
                    title = "Example 3",
                    private = true,
                    id = "3",
                ),
            ),
        )
        val actual = browserState.toTabStripState(
            isSelectDisabled = true,
            isPossiblyPrivateMode = true,
            addTab = {},
            toggleBrowsingMode = {},
            closeTab = { _, _ -> },
        )

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "2",
                    title = "Private Example",
                    url = "https://example2.com",
                    isSelected = false,
                    isPrivate = true,
                ),
                TabStripItem(
                    id = "3",
                    title = "Example 3",
                    url = "https://example3.com",
                    isSelected = false,
                    isPrivate = true,
                ),
            ),
            isPrivateMode = true,
            tabCounterMenuItems = noTabSelectedPrivateModeMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `GIVEN private mode is possibly on and select is not disabled WHEN selected tab is normal THEN tabs strip state tabs should include only normal tabs`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Private Example",
                    private = true,
                    id = "2",
                ),
                createTab(
                    url = "https://example3.com",
                    title = "Example 3",
                    private = true,
                    id = "3",
                ),
            ),
            selectedTabId = "1",
        )
        val actual = browserState.toTabStripState(
            isSelectDisabled = false,
            isPossiblyPrivateMode = true,
            addTab = {},
            toggleBrowsingMode = {},
            closeTab = { _, _ -> },
        )

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example",
                    url = "https://example.com",
                    isSelected = true,
                    isPrivate = false,
                ),
            ),
            isPrivateMode = false,
            tabCounterMenuItems = allMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `WHEN isSelectDisabled is false THEN tabs strip state tabs should have a selected tab`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Example 2",
                    private = false,
                    id = "2",
                ),
            ),
            selectedTabId = "2",
        )
        val actual = browserState.toTabStripState(
            isSelectDisabled = false,
            isPossiblyPrivateMode = false,
            addTab = {},
            toggleBrowsingMode = {},
            closeTab = { _, _ -> },
        )

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "2",
                    title = "Example 2",
                    url = "https://example2.com",
                    isSelected = true,
                    isPrivate = false,
                ),
            ),
            isPrivateMode = false,
            tabCounterMenuItems = allMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `WHEN isSelectDisabled is false and selected tab is private THEN tabs strip state tabs should have private tabs including the selected tab`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Example 2",
                    private = true,
                    id = "2",
                ),
                createTab(
                    url = "https://example3.com",
                    title = "Example 3",
                    private = true,
                    id = "3",
                ),
            ),
            selectedTabId = "2",
        )
        val actual = browserState.toTabStripState(
            isSelectDisabled = false,
            isPossiblyPrivateMode = false,
            addTab = {},
            toggleBrowsingMode = {},
            closeTab = { _, _ -> },
        )

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "2",
                    title = "Example 2",
                    url = "https://example2.com",
                    isSelected = true,
                    isPrivate = true,
                ),
                TabStripItem(
                    id = "3",
                    title = "Example 3",
                    url = "https://example3.com",
                    isSelected = false,
                    isPrivate = true,
                ),
            ),
            isPrivateMode = true,
            tabCounterMenuItems = allMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `WHEN isSelectDisabled is true THEN tabs strip state tabs should not have a selected tab`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "Example 2",
                    private = false,
                    id = "2",
                ),
            ),
            selectedTabId = "2",
        )
        val actual = browserState.toTabStripState(
            isSelectDisabled = true,
            isPossiblyPrivateMode = false,
            addTab = {},
            toggleBrowsingMode = {},
            closeTab = { _, _ -> },
        )

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "2",
                    title = "Example 2",
                    url = "https://example2.com",
                    isSelected = false,
                    isPrivate = false,
                ),
            ),
            isPrivateMode = false,
            tabCounterMenuItems = noTabSelectedNormalModeMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `WHEN a tab does not have a title THEN tabs strip should display the url`() {
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "",
                    private = false,
                    id = "2",
                ),
            ),
            selectedTabId = "2",
        )
        val actual = browserState.toTabStripState(
            isSelectDisabled = false,
            isPossiblyPrivateMode = false,
            addTab = {},
            toggleBrowsingMode = {},
            closeTab = { _, _ -> },
        )

        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "2",
                    title = "https://example2.com",
                    url = "https://example2.com",
                    isSelected = true,
                    isPrivate = false,
                ),
            ),
            isPrivateMode = false,
            tabCounterMenuItems = allMenuItems,
        )

        expected isSameAs actual
    }

    @Test
    fun `WHEN menu items are clicked THEN the correct action is performed`() {
        var addTabClicked = false
        var shouldOpenPrivateTab: Boolean? = null
        var toggleBrowsingModeClicked = false
        var closeTabClicked = false
        var closTabParams: Pair<Boolean, Int>? = null
        val browserState = BrowserState(
            tabs = listOf(
                createTab(
                    url = "https://example.com",
                    title = "Example 1",
                    private = false,
                    id = "1",
                ),
                createTab(
                    url = "https://example2.com",
                    title = "",
                    private = false,
                    id = "2",
                ),
            ),
            selectedTabId = "2",
        )
        val addTab = {
            addTabClicked = true
        }
        val toggleBrowsingMode: (isPrivate: Boolean) -> Unit = {
            toggleBrowsingModeClicked = true
            shouldOpenPrivateTab = it
        }
        val closeTab: (isPrivate: Boolean, numberOfTabs: Int) -> Unit = { isPrivate, numberOfTabs ->
            closeTabClicked = true
            closTabParams = Pair(isPrivate, numberOfTabs)
        }
        val actual = browserState.toTabStripState(
            isSelectDisabled = false,
            isPossiblyPrivateMode = false,
            addTab = addTab,
            toggleBrowsingMode = toggleBrowsingMode,
            closeTab = closeTab,
        )

        val newTab = TabCounterMenuItem.IconItem.NewTab(onClick = addTab)
        val newPrivateTab =
            TabCounterMenuItem.IconItem.NewPrivateTab(onClick = { toggleBrowsingMode(true) })
        val closeTabItem = TabCounterMenuItem.IconItem.CloseTab(onClick = { closeTab(false, 2) })
        val expected = TabStripState(
            tabs = listOf(
                TabStripItem(
                    id = "1",
                    title = "Example 1",
                    url = "https://example.com",
                    isSelected = false,
                    isPrivate = false,
                ),
                TabStripItem(
                    id = "2",
                    title = "https://example2.com",
                    url = "https://example2.com",
                    isSelected = true,
                    isPrivate = false,
                ),
            ),
            isPrivateMode = false,
            tabCounterMenuItems = listOf(
                newTab,
                newPrivateTab,
                TabCounterMenuItem.Divider,
                closeTabItem,
            ),
        )

        expected isSameAs actual

        newTab.onClick()
        assertEquals(true, addTabClicked)
        newPrivateTab.onClick()
        assertEquals(true, shouldOpenPrivateTab)
        assertEquals(true, toggleBrowsingModeClicked)
        closeTabItem.onClick()
        assertEquals(true, closeTabClicked)
        assertEquals(Pair(false, 2), closTabParams)
    }

    @Test
    fun `WHEN more than 7 tabs are present THEN close button should only be visible for the selected tab`() {
        val tab = createTab(
            url = "https://example.com",
            title = "Example",
            private = false,
            id = "1",
        )

        val tabs = List(8) {
            tab.copy(id = it.toString())
        }

        val browserState = BrowserState(
            tabs = tabs,
            selectedTabId = "1",
        )
        val actual =
            browserState.toTabStripState(
                isSelectDisabled = false,
                isPossiblyPrivateMode = false,
                addTab = {},
                toggleBrowsingMode = {},
                closeTab = { _, _ -> },
            )

        val tabStripItem = TabStripItem(
            id = "0",
            title = "Example",
            url = "https://example.com",
            isSelected = false,
            isPrivate = false,
            isCloseButtonVisible = false,
        )

        val expected = TabStripState(
            tabs = listOf(
                tabStripItem,
                TabStripItem(
                    id = "1",
                    title = "Example",
                    url = "https://example.com",
                    isSelected = true,
                    isPrivate = false,
                    isCloseButtonVisible = true,
                ),
                tabStripItem.copy(id = "2"),
                tabStripItem.copy(id = "3"),
                tabStripItem.copy(id = "4"),
                tabStripItem.copy(id = "5"),
                tabStripItem.copy(id = "6"),
                tabStripItem.copy(id = "7"),
            ),
            isPrivateMode = false,
            tabCounterMenuItems = allMenuItems,
        )

        expected isSameAs actual
    }

    /**
     * Asserts that the [TabStripState] is the same as the [other] [TabStripState] by comparing
     * their properties as assertEquals does. This ignores the lambda references in the
     * [TabCounterMenuItem.IconItem]s as asserting them is not straightforward.
     */
    private infix fun TabStripState.isSameAs(other: TabStripState) {
        assertEquals(tabs, other.tabs)
        assertEquals(isPrivateMode, other.isPrivateMode)
        assertEquals(
            tabCounterMenuItems.map { it.javaClass },
            other.tabCounterMenuItems.map { it.javaClass },
        )
    }

    private val allMenuItems = listOf(
        TabCounterMenuItem.IconItem.NewTab(onClick = {}),
        TabCounterMenuItem.IconItem.NewPrivateTab(onClick = {}),
        TabCounterMenuItem.Divider,
        TabCounterMenuItem.IconItem.CloseTab(onClick = {}),
    )

    private val noTabSelectedNormalModeMenuItems = listOf(
        TabCounterMenuItem.IconItem.NewPrivateTab(onClick = {}),
    )

    private val noTabSelectedPrivateModeMenuItems = listOf(
        TabCounterMenuItem.IconItem.NewTab(onClick = {}),
    )
}
