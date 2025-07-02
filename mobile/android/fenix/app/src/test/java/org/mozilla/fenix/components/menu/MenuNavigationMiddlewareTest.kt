/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.action.ShareResourceAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.content.ShareResourceState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.service.fxa.manager.AccountState.Authenticated
import mozilla.components.service.fxa.manager.AccountState.AuthenticationProblem
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.store.BookmarkState
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.AMO_HOMEPAGE_FOR_ANDROID
import org.mozilla.fenix.settings.SupportUtils.SumoTopic
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.webcompat.WEB_COMPAT_REPORTER_URL

class MenuNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val navController: NavController = mockk(relaxed = true)
    private val sessionUseCases: SessionUseCases = mockk(relaxed = true)
    private val webAppUseCases: WebAppUseCases = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)

    @Test
    fun `GIVEN account state is authenticated WHEN navigate to Mozilla account action is dispatched THEN dispatch navigate action to Mozilla account settings`() = runTest {
        val store = createStore()
        val accountState = Authenticated
        val accesspoint = MenuAccessPoint.Home

        store.dispatch(
            MenuAction.Navigate.MozillaAccount(
                accountState = accountState,
                accesspoint = accesspoint,
            ),
        ).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalAccountSettingsFragment(),
            )
        }
    }

    @Test
    fun `GIVEN account state is authentication problem WHEN navigate to Mozilla account action is dispatched THEN dispatch navigate action to Mozilla account problem`() = runTest {
        val store = createStore()
        val accountState = AuthenticationProblem
        val accesspoint = MenuAccessPoint.Home

        store.dispatch(
            MenuAction.Navigate.MozillaAccount(
                accountState = accountState,
                accesspoint = accesspoint,
            ),
        ).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalAccountProblemFragment(
                    entrypoint = FenixFxAEntryPoint.BrowserToolbar,
                ),
            )
        }
    }

    @Test
    fun `GIVEN account state is not authenticated WHEN navigate to Mozilla account action is dispatched THEN dispatch navigate action to turn on sync`() = runTest {
        val store = createStore()
        val accountState = NotAuthenticated
        val accesspoint = MenuAccessPoint.Home

        store.dispatch(
            MenuAction.Navigate.MozillaAccount(
                accountState = accountState,
                accesspoint = accesspoint,
            ),
        ).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalTurnOnSync(
                    entrypoint = FenixFxAEntryPoint.HomeMenu,
                ),
            )
        }
    }

    @Test
    fun `WHEN navigate to settings action is dispatched THEN navigate to settings`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalSettingsFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to bookmarks action is dispatched THEN navigate to bookmarks`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
            )
        }
    }

    @Test
    fun `WHEN navigate to history action is dispatched THEN navigate to history`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalHistoryFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to downloads action is dispatched THEN navigate to downloads`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalDownloadsFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to passwords action is dispatched THEN navigate to passwords`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Passwords).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionMenuDialogFragmentToLoginsListFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to customize homepage action is dispatched THEN navigate to homepage settings`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.CustomizeHomepage).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalHomeSettingsFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to release notes action is dispatched THEN navigate to SUMO topic`() = runTest {
        var params: BrowserNavigationParams? = null
        val store = createStore(
            openToBrowser = {
                params = it
            },
        )

        store.dispatch(MenuAction.Navigate.ReleaseNotes).join()

        assertEquals(SupportUtils.WHATS_NEW_URL, params?.url)
    }

    @Test
    fun `GIVEN current site is installable WHEN navigate to add to home screen is dispatched THEN invoke add to home screen use case`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
            onDismiss = { dismissWasCalled = true },
        )

        every { webAppUseCases.isInstallable() } returns true

        store.dispatch(MenuAction.Navigate.AddToHomeScreen).join()

        coVerify(exactly = 1) { webAppUseCases.addToHomescreen() }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN current site is not installable WHEN navigate to add to home screen is dispatched THEN navigate to create home screen shortcut fragment`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        every { webAppUseCases.isInstallable() } returns false

        store.dispatch(MenuAction.Navigate.AddToHomeScreen).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionMenuDialogFragmentToCreateShortcutFragment(),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.browserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `GIVEN there are existing tab collections WHEN navigate to save to collection action is dispatched THEN navigate to select collection creation`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.SaveToCollection(hasCollection = true)).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalCollectionCreationFragment(
                    tabIds = arrayOf(tab.id),
                    selectedTabIds = arrayOf(tab.id),
                    saveCollectionStep = SaveCollectionStep.SelectCollection,
                ),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.browserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `GIVEN there are no existing tab collections WHEN navigate to save to collection action is dispatched THEN navigate to new collection creation`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.SaveToCollection(hasCollection = false)).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalCollectionCreationFragment(
                    tabIds = arrayOf(tab.id),
                    selectedTabIds = arrayOf(tab.id),
                    saveCollectionStep = SaveCollectionStep.NameCollection,
                ),
            )
        }
    }

    @Test
    fun `WHEN navigate to edit bookmark action is dispatched THEN navigate to bookmark edit fragment`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                    bookmarkState = BookmarkState(
                        guid = BookmarkRoot.Mobile.id,
                        isBookmarked = true,
                    ),
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.EditBookmark).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalBookmarkEditFragment(
                    guidToEdit = BookmarkRoot.Mobile.id,
                    requiresSnackbarPaddingForToolbar = true,
                ),
            )
        }
    }

    @Test
    fun `WHEN navigate to translate action is dispatched THEN navigate to translation dialog`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.Translate).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionMenuDialogFragmentToTranslationsDialogFragment(),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.browserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `GIVEN reader view is active WHEN navigate to share action is dispatched THEN navigate to share sheet`() = runTest {
        val title = "Mozilla"
        val readerUrl = "moz-extension://1234"
        val activeUrl = "https://mozilla.org"
        val readerTab = createTab(
            url = readerUrl,
            readerState = ReaderState(active = true, activeUrl = activeUrl),
            title = title,
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = readerTab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.Share).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalShareFragment(
                    sessionId = readerTab.id,
                    data = arrayOf(
                        ShareData(
                            url = activeUrl,
                            title = title,
                        ),
                    ),
                    showPage = true,
                ),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.browserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `GIVEN reader view is inactive WHEN navigate to share action is dispatched THEN navigate to share sheet`() = runTest {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val tab = createTab(
            url = url,
            title = title,
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.Share).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalShareFragment(
                    sessionId = tab.id,
                    data = arrayOf(
                        ShareData(
                            url = url,
                            title = title,
                        ),
                    ),
                    showPage = true,
                ),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.browserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `GIVEN the current tab is a local PDF WHEN share menu item is pressed THEN trigger ShareResourceAction`() = runTest {
        val id = "1"
        val url = "content://pdf.pdf"
        val tab = createTab(
            url = url,
            id = id,
        )
        val browserStore = spyk(BrowserStore(BrowserState(tabs = listOf(tab), selectedTabId = id)))
        val store = createStore(
            browserStore = browserStore,
            customTab = null,
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.Share).join()

        verify {
            browserStore.dispatch(
                ShareResourceAction.AddShareAction(
                    id,
                    ShareResourceState.LocalResource(url),
                ),
            )
        }
    }

    @Test
    fun `GIVEN the current tab is a custom tab WHEN navigate to share action is dispatched THEN navigate to share sheet`() = runTest {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val customTab = CustomTabSessionState(
            content = ContentState(
                url = url,
                title = title,
            ),
            config = CustomTabConfig(),
        )
        val store = createStore(
            customTab = customTab,
            menuState = MenuState(),
        )

        store.dispatch(MenuAction.Navigate.Share).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalShareFragment(
                    sessionId = customTab.id,
                    data = arrayOf(
                        ShareData(
                            url = url,
                            title = title,
                        ),
                    ),
                    showPage = true,
                ),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.externalAppBrowserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `WHEN navigate to manage extensions action is dispatched THEN navigate to the extensions management`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.ManageExtensions).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalAddonsManagementFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to discover more extensions action is dispatched THEN navigate to the AMO page`() = runTest {
        var params: BrowserNavigationParams? = null
        val store = createStore(
            openToBrowser = {
                params = it
            },
        )

        store.dispatch(MenuAction.Navigate.DiscoverMoreExtensions).join()

        assertEquals(AMO_HOMEPAGE_FOR_ANDROID, params?.url)
    }

    @Test
    fun `WHEN navigate to extensions learn more action is dispatched THEN navigate to the SUMO page for installing add-ons`() = runTest {
        var params: BrowserNavigationParams? = null
        val store = createStore(
            openToBrowser = {
                params = it
            },
        )

        store.dispatch(MenuAction.Navigate.ExtensionsLearnMore).join()

        assertEquals(SumoTopic.FIND_INSTALL_ADDONS, params?.sumoTopic)
    }

    @Test
    fun `WHEN navigate to addon details is dispatched THEN navigate to the addon details`() = runTest {
        val addon = Addon(id = "ext1")
        val store = createStore()
        store.dispatch(MenuAction.Navigate.AddonDetails(addon = addon)).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionMenuDialogFragmenToAddonDetailsFragment(addon = addon),
            )
        }
    }

    @Test
    fun `GIVEN the user is on a tab WHEN the user clicks on the web compat button THEN navigate to the web compat reporter feature`() = runTest {
        every { settings.isTelemetryEnabled } returns true
        val expectedTabUrl = "www.mozilla.org"
        createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = createTab(
                        url = expectedTabUrl,
                    ),
                ),
            ),
        ).dispatch(MenuAction.Navigate.WebCompatReporter)

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionMenuDialogFragmentToWebCompatReporterFragment(tabUrl = expectedTabUrl),
            )
        }
    }

    @Test
    fun `GIVEN the user is on a tab WHEN the user clicks on the web compat button and telemetry is disabled THEN open browser`() = runTest {
        every { settings.isTelemetryEnabled } returns false
        var params: BrowserNavigationParams? = null
        val expectedTabUrl = "www.mozilla.org"
        val store = createStore(
            customTab = createCustomTab(
                url = expectedTabUrl,
            ),
            openToBrowser = {
                params = it
            },
        )

        store.dispatch(MenuAction.Navigate.WebCompatReporter).join()

        assertEquals("$WEB_COMPAT_REPORTER_URL$expectedTabUrl", params?.url)
    }

    @Test
    fun `GIVEN the user is on a custom tab WHEN the user clicks on the web compat button THEN navigate to the web compat reporter feature`() = runTest {
        every { settings.isTelemetryEnabled } returns true
        val expectedTabUrl = "www.mozilla.org"
        createStore(
            customTab = createCustomTab(
                url = expectedTabUrl,
            ),
        ).dispatch(MenuAction.Navigate.WebCompatReporter)

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionMenuDialogFragmentToWebCompatReporterFragment(tabUrl = expectedTabUrl),
            )
        }
    }

    @Test
    fun `GIVEN view history is true WHEN navigate back action is dispatched THEN navigate to tab history dialog fragment`() = runTest {
        val store = createStore(
            menuState = MenuState(
                customTabSessionId = "0",
            ),
        )

        store.dispatch(MenuAction.Navigate.Back(viewHistory = true)).join()

        verify {
            navController.nav(
                id = R.id.menuDialogFragment,
                directions = MenuDialogFragmentDirections.actionGlobalTabHistoryDialogFragment(
                    activeSessionId = store.state.customTabSessionId,
                ),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.browserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `GIVEN user is on a tab and view history is false WHEN navigate back action is dispatched THEN navigate back`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = null,
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Back(viewHistory = false)).join()

        verify {
            sessionUseCases.goBack.invoke(tab.id)
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN user is on a custom tab and view history is false WHEN navigate back action is dispatched THEN navigate back`() = runTest {
        val customTab = createCustomTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = customTab,
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Back(viewHistory = false)).join()

        verify {
            sessionUseCases.goBack.invoke(customTab.id)
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN view history is true WHEN navigate forward action is dispatched THEN navigate to tab history dialog fragment`() = runTest {
        val store = createStore(
            menuState = MenuState(
                customTabSessionId = "0",
            ),
        )

        store.dispatch(MenuAction.Navigate.Forward(viewHistory = true)).join()

        verify {
            navController.nav(
                id = R.id.menuDialogFragment,
                directions = MenuDialogFragmentDirections.actionGlobalTabHistoryDialogFragment(
                    activeSessionId = store.state.customTabSessionId,
                ),
                navOptions = NavOptions.Builder()
                    .setPopUpTo(R.id.browserFragment, false)
                    .build(),
            )
        }
    }

    @Test
    fun `GIVEN user is on a tab and view history is false WHEN navigate forward action is dispatched THEN navigate forward`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = null,
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Forward(viewHistory = false)).join()

        verify {
            sessionUseCases.goForward.invoke(tab.id)
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN user is on a custom tab and view history is false WHEN navigate forward action is dispatched THEN navigate forward`() = runTest {
        val customTab = createCustomTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = customTab,
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Forward(viewHistory = false)).join()

        verify {
            sessionUseCases.goForward.invoke(customTab.id)
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN bypass cache is true WHEN navigate reload action is dispatched THEN reload with bypass cache flag`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = null,
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Reload(bypassCache = true)).join()

        verify {
            sessionUseCases.reload.invoke(
                tabId = tab.id,
                flags = LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE),
            )
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN user is on a tab and bypass cache is false WHEN navigate reload action is dispatched THEN reload with no flags`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = null,
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Reload(bypassCache = false)).join()

        verify {
            sessionUseCases.reload.invoke(
                tabId = tab.id,
                flags = LoadUrlFlags.none(),
            )
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN user is on a custom tab and bypass cache is false WHEN navigate reload action is dispatched THEN reload with no flags`() = runTest {
        val customTab = createCustomTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = customTab,
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Reload(bypassCache = false)).join()

        verify {
            sessionUseCases.reload.invoke(
                tabId = customTab.id,
                flags = LoadUrlFlags.none(),
            )
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN user is on a tab WHEN navigate stop action is dispatched THEN stop loading the page`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = null,
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Stop).join()

        verify {
            sessionUseCases.stopLoading.invoke(tab.id)
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN user is on a custom tab WHEN navigate stop action is dispatched THEN stop loading the page`() = runTest {
        val customTab = createCustomTab(url = "https://www.mozilla.org")
        var dismissWasCalled = false
        val store = createStore(
            customTab = customTab,
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.Navigate.Stop).join()

        verify {
            sessionUseCases.stopLoading.invoke(customTab.id)
        }
        assertTrue(dismissWasCalled)
    }

    private fun createStore(
        browserStore: BrowserStore = mockk(relaxed = true),
        customTab: CustomTabSessionState? = mockk(relaxed = true),
        menuState: MenuState = MenuState(),
        openToBrowser: (params: BrowserNavigationParams) -> Unit = {},
        onDismiss: suspend () -> Unit = {},
    ) = MenuStore(
        initialState = menuState,
        middleware = listOf(
            MenuNavigationMiddleware(
                browserStore = browserStore,
                navController = navController,
                openToBrowser = openToBrowser,
                sessionUseCases = sessionUseCases,
                webAppUseCases = webAppUseCases,
                settings = settings,
                onDismiss = onDismiss,
                scope = scope,
                customTab = customTab,
            ),
        ),
    )
}
