/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import androidx.core.graphics.drawable.toDrawable
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.search.SearchEngine.Type.APPLICATION
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UpdateEditText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import org.mozilla.fenix.helpers.lifecycle.TestLifecycleOwner
import org.mozilla.fenix.search.BrowserToolbarSearchMiddleware.LifecycleDependencies
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorItemClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSettingsItemClicked
import org.mozilla.fenix.search.ext.searchEngineShortcuts
import org.robolectric.RobolectricTestRunner
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.ContentDescription as SearchSelectorDescription
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.Icon as SearchSelectorIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription as MenuItemDescription
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon as MenuItemIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text as MenuItemText

@RunWith(RobolectricTestRunner::class)
class BrowserToolbarSearchMiddlewareTest {
    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    val appStore: AppStore = mockk(relaxed = true)
    val browserStore: BrowserStore = mockk(relaxed = true) {
        every { state.search } returns fakeSearchState()
    }
    val lifecycleOwner: LifecycleOwner = TestLifecycleOwner(RESUMED)
    val navController: NavController = mockk {
        every { navigate(any<NavDirections>()) } just Runs
    }
    val resources: Resources = testContext.resources

    @Test
    fun `WHEN the toolbar enters in edit mode THEN a new search selector button is added`() {
        val (_, store) = buildMiddlewareAndAddToStore()

        store.dispatch(ToggleEditMode(true))

        assertSearchSelectorEquals(
            expectedSearchSelector(),
            store.state.editState.editActionsStart[0] as SearchSelectorAction,
        )
    }

    @Test
    fun `WHEN the search selector button is clicked THEN record a telemetry event`() {
        val (_, store) = buildMiddlewareAndAddToStore()

        store.dispatch(SearchSelectorClicked)

        assertNotNull(UnifiedSearch.searchMenuTapped.testGetValue())
    }

    @Test
    fun `GIVEN the search selector menu is open WHEN the search settings button is clicked THEN exit edit mode and open search settings`() {
        val (_, store) = buildMiddlewareAndAddToStore()
        store.dispatch(ToggleEditMode(true))
        store.dispatch(UpdateEditText("test"))
        assertTrue(store.state.isEditMode())
        assertEquals("test", store.state.editState.editText)

        store.dispatch(SearchSettingsItemClicked)

        assertFalse(store.state.isEditMode())
        assertEquals("", store.state.editState.editText)
        verify { appStore.dispatch(UpdateSearchBeingActiveState(false)) }
        verify { browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true)) }
        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalSearchEngineFragment(),
            )
        }
    }

    @Test
    fun `GIVEN the search selector menu is open WHEN a menu item is clicked THEN update the selected search engine and rebuild the menu`() {
        val (_, store) = buildMiddlewareAndAddToStore()
        val newEngineSelection = fakeSearchState().searchEngineShortcuts.last()
        store.dispatch(ToggleEditMode(true))
        assertSearchSelectorEquals(
            expectedSearchSelector(),
            store.state.editState.editActionsStart[0] as SearchSelectorAction,
        )

        store.dispatch(SearchSelectorItemClicked(newEngineSelection))

        assertSearchSelectorEquals(
            expectedSearchSelector(newEngineSelection),
            store.state.editState.editActionsStart[0] as SearchSelectorAction,
        )
    }

    /**
     * Assert the [expected] search selector (and its menu items) is the same as [actual]
     * with special support for verifying the icons are the same.
     */
    private fun assertSearchSelectorEquals(
        expected: SearchSelectorAction,
        actual: SearchSelectorAction,
    ) {
        when (expected.icon) {
            is SearchSelectorIcon.DrawableIcon -> {
                assertEquals(
                    ((expected.icon as SearchSelectorIcon.DrawableIcon).drawable as BitmapDrawable).state,
                    ((actual.icon as SearchSelectorIcon.DrawableIcon).drawable as BitmapDrawable).state,
                )
                assertEquals(
                    (expected.icon as SearchSelectorIcon.DrawableIcon).shouldTint,
                    (actual.icon as SearchSelectorIcon.DrawableIcon).shouldTint,
                )
            }
            is SearchSelectorIcon.DrawableResIcon -> {
                assertEquals(
                    (expected.icon as SearchSelectorIcon.DrawableResIcon).resourceId,
                    (actual.icon as SearchSelectorIcon.DrawableResIcon).resourceId,
                )
            }
        }
        assertEquals(expected.contentDescription, actual.contentDescription)
        assertEquals(expected.onClick, actual.onClick)

        val expectedMenuItems = expected.menu.items()
        val actualMenuItems = actual.menu.items()
        assertEquals(expectedMenuItems.size, actualMenuItems.size)
        for (i in expectedMenuItems.indices) {
            val expectedMenuItem = expectedMenuItems[i] as BrowserToolbarMenuButton
            val actualMenuItem = actualMenuItems[i] as BrowserToolbarMenuButton

            when (expectedMenuItem.icon) {
                is MenuItemIcon.DrawableIcon -> {
                    assertEquals(
                        ((expectedMenuItem.icon as MenuItemIcon.DrawableIcon).drawable as BitmapDrawable).state,
                        ((actualMenuItem.icon as MenuItemIcon.DrawableIcon).drawable as BitmapDrawable).state,
                    )
                    assertEquals(
                        (expectedMenuItem.icon as MenuItemIcon.DrawableIcon).shouldTint,
                        (actualMenuItem.icon as MenuItemIcon.DrawableIcon).shouldTint,
                    )
                }
                is MenuItemIcon.DrawableResIcon -> {
                    assertEquals(
                        (expectedMenuItem.icon as MenuItemIcon.DrawableResIcon).resourceId,
                        (actualMenuItem.icon as MenuItemIcon.DrawableResIcon).resourceId,
                    )
                }
                null -> assertNull(actualMenuItem.icon)
            }
            assertEquals(expectedMenuItem.contentDescription, actualMenuItem.contentDescription)

            val expectedSearchEngineClickEvent = (expectedMenuItem.onClick as? SearchSelectorItemClicked)?.searchEngine
            val actualSearchEngineClickEvent = (actualMenuItem.onClick as? SearchSelectorItemClicked)?.searchEngine
            if (expectedSearchEngineClickEvent == null) {
                assertNull(actualSearchEngineClickEvent)
            } else {
                assertSearchEngineEquals(expectedSearchEngineClickEvent, actualSearchEngineClickEvent!!)
            }
        }
    }

    private fun assertSearchEngineEquals(
        expected: SearchEngine,
        actual: SearchEngine,
    ) {
        assertEquals(expected.id, actual.id)
        assertEquals(expected.name, actual.name)
        assertEquals(expected.icon.rowBytes, actual.icon.rowBytes)
        assertEquals(expected.inputEncoding, actual.inputEncoding)
        assertEquals(expected.type, actual.type)
        assertEquals(expected.resultUrls, actual.resultUrls)
        assertEquals(expected.suggestUrl, actual.suggestUrl)
        assertEquals(expected.trendingUrl, actual.trendingUrl)
        assertEquals(expected.isGeneral, actual.isGeneral)
    }

    private fun expectedSearchSelector(
        defaultOrSelectedSearchEngine: SearchEngine = fakeSearchState().selectedOrDefaultSearchEngine!!,
        searchEngineShortcuts: List<SearchEngine> = fakeSearchState().searchEngineShortcuts,
    ) = SearchSelectorAction(
        icon = SearchSelectorIcon.DrawableIcon(
            drawable = defaultOrSelectedSearchEngine.icon.toDrawable(resources),
            shouldTint = defaultOrSelectedSearchEngine.type == APPLICATION,
        ),
        contentDescription = SearchSelectorDescription.StringContentDescription(
            "${defaultOrSelectedSearchEngine.name}: search engine selector",
        ),
        menu = BrowserToolbarMenu { expectedSearchSelectorMenuItems(searchEngineShortcuts) },
        onClick = SearchSelectorClicked,
    )

    private fun expectedSearchSelectorMenuItems(searchEnginesShortcuts: List<SearchEngine>) = buildList {
        add(
            BrowserToolbarMenuButton(
                icon = null,
                text = MenuItemText.StringResText(R.string.search_header_menu_item_2),
                contentDescription = MenuItemDescription.StringResContentDescription(R.string.search_header_menu_item_2),
                onClick = null,
            ),
        )
        addAll(
            searchEnginesShortcuts.map { searchEngine ->
                BrowserToolbarMenuButton(
                    icon = MenuItemIcon.DrawableIcon(
                        drawable = searchEngine.icon.toDrawable(resources),
                        shouldTint = searchEngine.type == APPLICATION,
                    ),
                    text = MenuItemText.StringText(searchEngine.name),
                    contentDescription = MenuItemDescription.StringContentDescription(searchEngine.name),
                    onClick = SearchSelectorItemClicked(searchEngine),
                )
            },
        )
        add(
            BrowserToolbarMenuButton(
                icon = MenuItemIcon.DrawableResIcon(R.drawable.mozac_ic_settings_24),
                text = MenuItemText.StringResText(R.string.search_settings_menu_item),
                contentDescription = MenuItemDescription.StringResContentDescription(R.string.search_settings_menu_item),
                onClick = SearchSettingsItemClicked,
            ),
        )
    }

    private fun buildMiddlewareAndAddToStore(
        appStore: AppStore = this.appStore,
        browserStore: BrowserStore = this.browserStore,
        lifecycleOwner: LifecycleOwner = this.lifecycleOwner,
        navController: NavController = this.navController,
        resources: Resources = this.resources,
    ): Pair<BrowserToolbarSearchMiddleware, BrowserToolbarStore> {
        val middleware = buildMiddleware(appStore, browserStore, lifecycleOwner, navController, resources)
        val store = BrowserToolbarStore(middleware = listOf(middleware))

        return middleware to store
    }

    private fun buildMiddleware(
        appStore: AppStore = this.appStore,
        browserStore: BrowserStore = this.browserStore,
        lifecycleOwner: LifecycleOwner = this.lifecycleOwner,
        navController: NavController = this.navController,
        resources: Resources = this.resources,
    ) = BrowserToolbarSearchMiddleware(appStore, browserStore).apply {
        updateLifecycleDependencies(
            LifecycleDependencies(
                lifecycleOwner, navController, resources,
            ),
        )
    }

    private fun fakeSearchState() = SearchState(
        region = RegionState("US", "US"),
        regionSearchEngines = listOf(
            SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
            SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
        ),
        customSearchEngines = listOf(
            SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.CUSTOM),
        ),
        applicationSearchEngines = listOf(
            SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.APPLICATION),
        ),
        additionalSearchEngines = listOf(
            SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
        ),
        additionalAvailableSearchEngines = listOf(
            SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
        ),
        hiddenSearchEngines = listOf(
            SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED),
        ),
        regionDefaultSearchEngineId = null,
        userSelectedSearchEngineId = null,
        userSelectedSearchEngineName = null,
    )
}
