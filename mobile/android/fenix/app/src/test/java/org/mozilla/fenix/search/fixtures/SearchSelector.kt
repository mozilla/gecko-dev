/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.fixtures

import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import androidx.core.graphics.drawable.toDrawable
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.search.SearchEngine.Type.APPLICATION
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.mozilla.fenix.R
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorItemClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSettingsItemClicked
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.ContentDescription as SearchSelectorDescription
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.Icon as SearchSelectorIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription as MenuItemDescription
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon as MenuItemIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text as MenuItemText

/**
 * Assert the [expected] search selector (and its menu items) is the same as [actual]
 * with special support for verifying the icons are the same.
 */
fun assertSearchSelectorEquals(
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

fun assertSearchEngineEquals(
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

fun buildExpectedSearchSelector(
    defaultOrSelectedSearchEngine: SearchEngine,
    searchEngineShortcuts: List<SearchEngine>,
    resources: Resources,
) = SearchSelectorAction(
    icon = SearchSelectorIcon.DrawableIcon(
        drawable = defaultOrSelectedSearchEngine.icon.toDrawable(resources),
        shouldTint = defaultOrSelectedSearchEngine.type == APPLICATION,
    ),
    contentDescription = SearchSelectorDescription.StringContentDescription(
        "${defaultOrSelectedSearchEngine.name}: search engine selector",
    ),
    menu = BrowserToolbarMenu { buildExpectedSearchSelectorMenuItems(searchEngineShortcuts, resources) },
    onClick = SearchSelectorClicked,
)

fun buildExpectedSearchSelectorMenuItems(
    searchEnginesShortcuts: List<SearchEngine>,
    resources: Resources,
) = buildList {
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
