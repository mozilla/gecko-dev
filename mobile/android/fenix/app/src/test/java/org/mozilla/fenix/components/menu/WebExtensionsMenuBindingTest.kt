/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore

@RunWith(AndroidJUnit4::class)
class WebExtensionsMenuBindingTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()

    lateinit var browserStore: BrowserStore
    private lateinit var menuStore: MenuStore

    @Test
    fun `WHEN web extension state get updated in the browserStore THEN invoke action update web extension menu items`() =
        runTestOnMain {
            val defaultPageAction = createWebExtensionPageAction("default_page_action_title")

            val overriddenPageAction = createWebExtensionPageAction("overridden_page_action_title")

            val defaultBrowserAction =
                createWebExtensionBrowserAction("default_browser_action_title")

            val overriddenBrowserAction =
                createWebExtensionBrowserAction("overridden_browser_action_title")

            val extensions: Map<String, WebExtensionState> = mapOf(
                "id" to WebExtensionState(
                    id = "id",
                    url = "url",
                    name = "name",
                    enabled = true,
                    browserAction = defaultBrowserAction,
                    pageAction = defaultPageAction,
                ),
            )
            val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
                "id" to WebExtensionState(
                    id = "id",
                    url = "url",
                    name = "name",
                    enabled = true,
                    browserAction = overriddenBrowserAction,
                    pageAction = overriddenPageAction,
                ),
            )

            menuStore = spy(MenuStore(MenuState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(
                            url = "https://www.example.org",
                            id = "tab1",
                            extensions = overriddenExtensions,
                        ),
                    ),
                    selectedTabId = "tab1",
                    extensions = extensions,
                ),
            )

            val binding = WebExtensionsMenuBinding(
                browserStore = browserStore,
                menuStore = menuStore,
                iconSize = 24.dpToPx(testContext.resources.displayMetrics),
                onDismiss = {},
            )
            binding.start()

            val itemsUpdateCaptor = argumentCaptor<MenuAction.UpdateWebExtensionMenuItems>()

            verify(menuStore).dispatch(itemsUpdateCaptor.capture())
            assertEquals(
                itemsUpdateCaptor.value.webExtensionMenuItems[0].label,
                "overridden_browser_action_title",
            )
            assertTrue(itemsUpdateCaptor.value.webExtensionMenuItems[0].enabled == true)
            assertEquals(itemsUpdateCaptor.value.webExtensionMenuItems[0].badgeText, "")
            assertEquals(itemsUpdateCaptor.value.webExtensionMenuItems[0].badgeTextColor, 0)
            assertEquals(itemsUpdateCaptor.value.webExtensionMenuItems[0].badgeBackgroundColor, 0)
        }

    private fun createWebExtensionPageAction(title: String) = WebExtensionPageAction(
        title = title,
        enabled = true,
        loadIcon = mock(),
        badgeText = "",
        badgeTextColor = 0,
        badgeBackgroundColor = 0,
        onClick = {},
    )

    private fun createWebExtensionBrowserAction(title: String) = WebExtensionBrowserAction(
        title,
        enabled = true,
        loadIcon = mock(),
        badgeText = "",
        badgeTextColor = 0,
        badgeBackgroundColor = 0,
        onClick = {},
    )
}
