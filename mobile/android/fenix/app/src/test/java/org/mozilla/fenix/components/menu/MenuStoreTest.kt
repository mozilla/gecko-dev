/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.addons.Addon
import mozilla.components.lib.state.Middleware
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.menu.store.BookmarkState
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.ExtensionMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.components.menu.store.WebExtensionMenuItem
import org.mozilla.fenix.components.menu.store.copyWithBrowserMenuState
import org.mozilla.fenix.components.menu.store.copyWithExtensionMenuState

class MenuStoreTest {

    @Test
    fun `WHEN store is created THEN init action is dispatched`() {
        var initActionObserved = false
        val testMiddleware: Middleware<MenuState, MenuAction> = { _, next, action ->
            if (action == MenuAction.InitAction) {
                initActionObserved = true
            }

            next(action)
        }

        val store = MenuStore(
            initialState = MenuState(),
            middleware = listOf(testMiddleware),
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        assertTrue(initActionObserved)
        assertNull(store.state.browserMenuState)
    }

    @Test
    fun `GIVEN a browser menu state update WHEN copying the browser menu state THEN return the updated browser menu state`() {
        val selectedTab = TabSessionState(
            id = "tabId1",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )
        val firefoxTab = TabSessionState(
            id = "tabId2",
            content = ContentState(
                url = "www.firefox.com",
            ),
        )
        val state = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = selectedTab,
                bookmarkState = BookmarkState(),
                isPinned = false,
            ),
        )

        assertEquals(selectedTab, state.browserMenuState!!.selectedTab)
        assertNull(state.browserMenuState!!.bookmarkState.guid)
        assertFalse(state.browserMenuState!!.bookmarkState.isBookmarked)
        assertFalse(state.browserMenuState!!.isPinned)

        var newState = state.copyWithBrowserMenuState {
            it.copy(selectedTab = firefoxTab)
        }

        assertEquals(firefoxTab, newState.browserMenuState!!.selectedTab)
        assertNull(state.browserMenuState!!.bookmarkState.guid)
        assertFalse(state.browserMenuState!!.bookmarkState.isBookmarked)
        assertFalse(state.browserMenuState!!.isPinned)

        val bookmarkState = BookmarkState(guid = "id", isBookmarked = true)
        val isPinned = true
        newState = newState.copyWithBrowserMenuState {
            it.copy(bookmarkState = bookmarkState, isPinned = isPinned)
        }

        assertEquals(firefoxTab, newState.browserMenuState!!.selectedTab)
        assertEquals(bookmarkState, newState.browserMenuState!!.bookmarkState)
        assertEquals(isPinned, newState.browserMenuState!!.isPinned)
    }

    @Test
    fun `GIVEN an extension menu state update WHEN copying the extension menu state THEN return the updated extension menu state`() {
        val addon = Addon(id = "ext1")
        val state = MenuState()

        assertEquals(0, state.extensionMenuState.recommendedAddons.size)

        val newState = state.copyWithExtensionMenuState {
            it.copy(recommendedAddons = listOf(addon))
        }

        assertEquals(1, newState.extensionMenuState.recommendedAddons.size)
        assertEquals(addon, newState.extensionMenuState.recommendedAddons.first())
    }

    @Test
    fun `WHEN add bookmark action is dispatched THEN state is not updated`() = runTest {
        val initialState = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "www.google.com",
                    ),
                ),
                bookmarkState = BookmarkState(),
            ),
        )
        val store = MenuStore(initialState = initialState)

        store.dispatch(MenuAction.AddBookmark).join()

        assertEquals(initialState, store.state)
    }

    @Test
    fun `WHEN update bookmark state action is dispatched THEN bookmark state is updated`() = runTest {
        val initialState = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "www.google.com",
                    ),
                ),
                bookmarkState = BookmarkState(),
            ),
        )
        val store = MenuStore(initialState = initialState)

        assertNotNull(store.state.browserMenuState)
        assertNull(store.state.browserMenuState!!.bookmarkState.guid)
        assertFalse(store.state.browserMenuState!!.bookmarkState.isBookmarked)

        val newBookmarkState = BookmarkState(
            guid = "id1",
            isBookmarked = true,
        )
        store.dispatch(MenuAction.UpdateBookmarkState(bookmarkState = newBookmarkState)).join()

        assertEquals(newBookmarkState, store.state.browserMenuState!!.bookmarkState)
    }

    @Test
    fun `WHEN add shortcut action is dispatched THEN state is not updated`() = runTest {
        val initialState = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "www.google.com",
                    ),
                ),
                isPinned = false,
            ),
        )
        val store = MenuStore(initialState = initialState)

        store.dispatch(MenuAction.AddShortcut).join()

        assertEquals(initialState, store.state)
    }

    @Test
    fun `WHEN remove shortcut action is dispatched THEN state is not updated`() = runTest {
        val initialState = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "www.google.com",
                    ),
                ),
                isPinned = false,
            ),
        )
        val store = MenuStore(initialState = initialState)

        store.dispatch(MenuAction.RemoveShortcut).join()

        assertEquals(initialState, store.state)
    }

    @Test
    fun `WHEN update shortcut state action is dispatched THEN pinned state is updated`() = runTest {
        val initialState = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "www.google.com",
                    ),
                ),
                isPinned = false,
            ),
        )
        val store = MenuStore(initialState = initialState)

        assertNotNull(store.state.browserMenuState)
        assertFalse(store.state.browserMenuState!!.isPinned)

        store.dispatch(MenuAction.UpdatePinnedState(isPinned = true)).join()
        assertTrue(store.state.browserMenuState!!.isPinned)
    }

    @Test
    fun `WHEN update extension state action is dispatched THEN extension state is updated`() = runTest {
        val addon = Addon(id = "ext1")
        val store = MenuStore(initialState = MenuState())

        assertEquals(0, store.state.extensionMenuState.recommendedAddons.size)

        store.dispatch(MenuAction.UpdateExtensionState(recommendedAddons = listOf(addon))).join()

        assertEquals(1, store.state.extensionMenuState.recommendedAddons.size)
        assertEquals(addon, store.state.extensionMenuState.recommendedAddons.first())
    }

    @Test
    fun `WHEN find in page action is dispatched THEN state is not updated`() = runTest {
        val initialState = MenuState()
        val store = MenuStore(initialState = initialState)

        store.dispatch(MenuAction.FindInPage).join()

        assertEquals(initialState, store.state)
    }

    @Test
    fun `WHEN request desktop site action is dispatched THEN desktop mode state is updated`() = runTest {
        val initialState = MenuState()
        val store = MenuStore(initialState = initialState)

        store.dispatch(MenuAction.RequestDesktopSite).join()

        assertTrue(store.state.isDesktopMode)
    }

    @Test
    fun `WHEN request mobile site action is dispatched THEN desktop mode state is updated`() = runTest {
        val initialState = MenuState(isDesktopMode = true)
        val store = MenuStore(initialState = initialState)

        store.dispatch(MenuAction.RequestMobileSite).join()

        assertFalse(store.state.isDesktopMode)
    }

    @Test
    fun `WHEN addon installation is in progress action is dispatched THEN extension state is updated`() =
        runTest {
            val addon = Addon(id = "ext1")
            val store = MenuStore(initialState = MenuState())

            store.dispatch(MenuAction.UpdateInstallAddonInProgress(addon)).join()

            assertEquals(addon, store.state.extensionMenuState.addonInstallationInProgress)
        }

    @Test
    fun `WHEN addon installation with success action is dispatched THEN extension state is updated`() =
        runTest {
            val addon = Addon(id = "ext1")
            val addonTwo = Addon(id = "ext2")
            val store = MenuStore(
                initialState = MenuState(
                    extensionMenuState = ExtensionMenuState(
                        recommendedAddons = listOf(
                            addon,
                            addonTwo,
                        ),
                    ),
                ),
            )

            store.dispatch(MenuAction.InstallAddonSuccess(addon)).join()

            assertEquals(null, store.state.extensionMenuState.addonInstallationInProgress)
            assertEquals(1, store.state.extensionMenuState.recommendedAddons.size)
        }

    @Test
    fun `WHEN addon installation failed action is dispatched THEN extension state is updated`() =
        runTest {
            val addon = Addon(id = "ext1")
            val addonTwo = Addon(id = "ext2")
            val store = MenuStore(
                initialState = MenuState(
                    extensionMenuState = ExtensionMenuState(
                        recommendedAddons = listOf(
                            addon,
                            addonTwo,
                        ),
                    ),
                ),
            )

            store.dispatch(MenuAction.InstallAddonFailed(addon)).join()

            assertEquals(null, store.state.extensionMenuState.addonInstallationInProgress)
            assertEquals(2, store.state.extensionMenuState.recommendedAddons.size)
        }

    @Test
    fun `WHEN update web extension menu items is dispatched THEN extension state is updated`() =
        runTest {
            val initialState = MenuState()
            val store = MenuStore(initialState = initialState)
            val webExtensionMenuItemList = listOf(
                WebExtensionMenuItem(
                    label = "label",
                    enabled = true,
                    icon = null,
                    badgeText = "1",
                    badgeTextColor = Color.White.toArgb(),
                    badgeBackgroundColor = Color.Gray.toArgb(),
                    onClick = {
                    },
                ),
            )
            store.dispatch(MenuAction.UpdateWebExtensionMenuItems(webExtensionMenuItemList)).join()

            assertEquals(
                store.state.extensionMenuState.webExtensionMenuItems,
                webExtensionMenuItemList,
            )
        }
}
