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
import org.junit.Assert.assertFalse
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
    fun `WHEN browser web extension state get updated in the browserStore THEN invoke action update browser web extension menu items`() =
        runTestOnMain {
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
                ),
            )
            val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
                "id" to WebExtensionState(
                    id = "id",
                    url = "url",
                    name = "name",
                    enabled = true,
                    browserAction = overriddenBrowserAction,
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

            val browserItemsUpdateCaptor = argumentCaptor<MenuAction.UpdateWebExtensionBrowserMenuItems>()

            verify(menuStore).dispatch(browserItemsUpdateCaptor.capture())
            assertEquals(
                browserItemsUpdateCaptor.value.webExtensionBrowserMenuItem[0].label,
                "overridden_browser_action_title",
            )
            assertTrue(browserItemsUpdateCaptor.value.webExtensionBrowserMenuItem[0].enabled == true)
            assertEquals(browserItemsUpdateCaptor.value.webExtensionBrowserMenuItem[0].badgeText, "")
            assertEquals(browserItemsUpdateCaptor.value.webExtensionBrowserMenuItem[0].badgeTextColor, 0)
            assertEquals(browserItemsUpdateCaptor.value.webExtensionBrowserMenuItem[0].badgeBackgroundColor, 0)
        }

    @Test
    fun `WHEN all web extensions are disabled THEN show disabled extensions onboarding`() =
        runTestOnMain {
            val extensions: Map<String, WebExtensionState> = mapOf(
                "id" to WebExtensionState(
                    id = "id",
                    url = "url",
                    name = "name",
                    enabled = false,
                ),
            )

            menuStore = spy(MenuStore(MenuState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(
                            url = "https://www.example.org",
                            id = "tab1",
                            extensions = extensions,
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

            val showDisabledExtensionsOnboardingCaptor = argumentCaptor<MenuAction.UpdateShowDisabledExtensionsOnboarding>()

            verify(menuStore).dispatch(showDisabledExtensionsOnboardingCaptor.capture())

            assertTrue(showDisabledExtensionsOnboardingCaptor.value.showDisabledExtensionsOnboarding)
        }

    @Test
    fun `WHEN only one web extension is disabled THEN not show disabled extensions onboarding`() =
        runTestOnMain {
            val extensions: Map<String, WebExtensionState> = mapOf(
                "id" to WebExtensionState(
                    id = "id",
                    url = "url",
                    name = "name",
                    enabled = false,
                ),
                "id2" to WebExtensionState(
                    id = "id2",
                    url = "url2",
                    name = "name2",
                    enabled = true,
                ),
            )

            menuStore = spy(MenuStore(MenuState()))
            browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(
                            url = "https://www.example.org",
                            id = "tab1",
                            extensions = extensions,
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

            val showDisabledExtensionsOnboardingCaptor = argumentCaptor<MenuAction.UpdateShowDisabledExtensionsOnboarding>()

            verify(menuStore).dispatch(showDisabledExtensionsOnboardingCaptor.capture())

            assertFalse(showDisabledExtensionsOnboardingCaptor.value.showDisabledExtensionsOnboarding)
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
