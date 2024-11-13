/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import android.app.AlertDialog
import android.app.PendingIntent
import android.content.Intent
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.webextension.InstallationMethod
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.app.links.AppLinkRedirect
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSitesUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.BookmarkAction
import org.mozilla.fenix.components.appstate.AppAction.FindInPageAction
import org.mozilla.fenix.components.appstate.AppAction.ReaderViewAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase.AddBookmarksUseCase
import org.mozilla.fenix.components.menu.fake.FakeBookmarksStorage
import org.mozilla.fenix.components.menu.middleware.MenuDialogMiddleware
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class MenuDialogMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val bookmarksStorage = FakeBookmarksStorage()
    private val addBookmarkUseCase: AddBookmarksUseCase =
        spy(AddBookmarksUseCase(storage = bookmarksStorage))

    private val addonManager: AddonManager = mock()
    private val onDeleteAndQuit: () -> Unit = mock()

    private lateinit var alertDialogBuilder: AlertDialog.Builder
    private lateinit var pinnedSiteStorage: PinnedSiteStorage
    private lateinit var addPinnedSiteUseCase: TopSitesUseCases.AddPinnedSiteUseCase
    private lateinit var removePinnedSiteUseCase: TopSitesUseCases.RemoveTopSiteUseCase
    private lateinit var appLinksUseCases: AppLinksUseCases
    private lateinit var requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase
    private lateinit var settings: Settings

    companion object {
        const val TOP_SITES_MAX_COUNT = 16
    }

    @Before
    fun setup() {
        alertDialogBuilder = mock()
        pinnedSiteStorage = mock()
        addPinnedSiteUseCase = mock()
        removePinnedSiteUseCase = mock()
        appLinksUseCases = mock()
        requestDesktopSiteUseCase = mock()

        settings = Settings(testContext)

        runBlocking {
            whenever(pinnedSiteStorage.getPinnedSites()).thenReturn(emptyList())
            whenever(addonManager.getAddons()).thenReturn(emptyList())
        }
    }

    @Test
    fun `GIVEN no selected tab WHEN init action is dispatched THEN browser state is not updated`() = runTestOnMain {
        val store = createStore()

        assertNull(store.state.browserMenuState)

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        assertNull(store.state.browserMenuState)
    }

    @Test
    fun `GIVEN selected tab is bookmarked WHEN init action is dispatched THEN initial bookmark state is updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"

        val guid = bookmarksStorage.addItem(
            parentGuid = BookmarkRoot.Mobile.id,
            url = url,
            title = title,
            position = 5u,
        )

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        // Wait for UpdateBookmarkState and middleware
        store.waitUntilIdle()

        assertEquals(guid, store.state.browserMenuState!!.bookmarkState.guid)
        assertTrue(store.state.browserMenuState!!.bookmarkState.isBookmarked)
    }

    @Test
    fun `GIVEN selected tab is not bookmarked WHEN init action is dispatched THEN initial bookmark state is not updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
        )

        assertNull(store.state.browserMenuState!!.bookmarkState.guid)
        assertFalse(store.state.browserMenuState!!.bookmarkState.isBookmarked)

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        assertNull(store.state.browserMenuState!!.bookmarkState.guid)
        assertFalse(store.state.browserMenuState!!.bookmarkState.isBookmarked)
    }

    @Test
    fun `GIVEN recommended addons are available WHEN init action is dispatched THEN initial extension state is updated`() = runTestOnMain {
        val addon = Addon(id = "ext1")
        whenever(addonManager.getAddons()).thenReturn(listOf(addon))

        val store = createStore()

        assertEquals(0, store.state.extensionMenuState.recommendedAddons.size)

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        // Wait for UpdateExtensionState and middleware
        store.waitUntilIdle()

        assertTrue(store.state.extensionMenuState.availableAddons.isEmpty())
        assertEquals(1, store.state.extensionMenuState.recommendedAddons.size)
        assertEquals(addon, store.state.extensionMenuState.recommendedAddons.first())
        assertTrue(store.state.extensionMenuState.showExtensionsOnboarding)
    }

    @Test
    fun `GIVEN at least one addon is installed WHEN init action is dispatched THEN initial extension state is updated`() =
        runTestOnMain {
            val addon = Addon(id = "ext1")
            val addonTwo = Addon(
                id = "ext2",
                installedState = Addon.InstalledState(
                    id = "id",
                    version = "1.0",
                    enabled = true,
                    optionsPageUrl = "",
                ),
            )
            val addonThree = Addon(id = "ext3")
            whenever(addonManager.getAddons()).thenReturn(listOf(addon, addonTwo, addonThree))

            val store = createStore()

            assertEquals(0, store.state.extensionMenuState.recommendedAddons.size)

            // Wait for InitAction and middleware
            store.waitUntilIdle()

            // Wait for UpdateExtensionState and middleware
            store.waitUntilIdle()

            assertEquals(1, store.state.extensionMenuState.availableAddons.size)
            assertTrue(store.state.extensionMenuState.recommendedAddons.isEmpty())
            assertFalse(store.state.extensionMenuState.showExtensionsOnboarding)
            assertTrue(store.state.extensionMenuState.shouldShowManageExtensionsMenuItem)
        }

    @Test
    fun `GIVEN at least one addon is installed and not enabled WHEN init action is dispatched THEN initial extension state is updated`() =
        runTestOnMain {
            val addon = Addon(
                id = "ext",
                installedState = Addon.InstalledState(
                    id = "id",
                    version = "1.0",
                    enabled = false,
                    optionsPageUrl = "",
                ),
            )

            whenever(addonManager.getAddons()).thenReturn(listOf(addon))

            val store = createStore()

            assertEquals(0, store.state.extensionMenuState.recommendedAddons.size)

            // Wait for InitAction and middleware
            store.waitUntilIdle()

            // Wait for UpdateExtensionState and middleware
            store.waitUntilIdle()

            assertTrue(store.state.extensionMenuState.availableAddons.isEmpty())
            assertTrue(store.state.extensionMenuState.recommendedAddons.isEmpty())
            assertFalse(store.state.extensionMenuState.showExtensionsOnboarding)
            assertTrue(store.state.extensionMenuState.shouldShowManageExtensionsMenuItem)
        }

    @Test
    fun `WHEN add bookmark action is dispatched for a selected tab THEN bookmark is added`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val captureMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
        val appStore = AppStore(middlewares = listOf(captureMiddleware))
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.AddBookmark)
        store.waitUntilIdle()

        verify(addBookmarkUseCase).invoke(url = url, title = title, parentGuid = BookmarkRoot.Mobile.id)
        captureMiddleware.assertLastAction(BookmarkAction.BookmarkAdded::class) { action: BookmarkAction.BookmarkAdded ->
            assertNotNull(action.guidToEdit)
        }
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN the last added bookmark belongs to a folder WHEN bookmark is added THEN bookmark is added to folder and parent title is dispatched`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val parentTitle = "title"

        // Add parent
        val parentGuid = bookmarksStorage.addItem(
            parentGuid = "root",
            url = "url",
            title = parentTitle,
            position = 0U,
        )
        // Add pre-existing child
        bookmarksStorage.addItem(
            parentGuid = parentGuid,
            url = "url",
            title = parentTitle,
            position = 1U,
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val captureMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
        val appStore = AppStore(middlewares = listOf(captureMiddleware))
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { },
        )

        store.dispatch(MenuAction.AddBookmark)
        store.waitUntilIdle()

        verify(addBookmarkUseCase).invoke(url = url, title = title, parentGuid = parentGuid)
        captureMiddleware.assertLastAction(BookmarkAction.BookmarkAdded::class) { action: BookmarkAction.BookmarkAdded ->
            assertNotNull(action.guidToEdit)
            assertEquals(parentTitle, action.parentNode?.title)
        }
    }

    @Test
    fun `GIVEN the last added bookmark does not belongs to a folder WHEN bookmark is added THEN bookmark is added to mobile root`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"

        // Add a pre-existing item. This accounts for the null case, but that shouldn't actually be
        // possible because the mobile root is a subfolder of the synced root
        bookmarksStorage.addFolder(
            parentGuid = "",
            title = "title",
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = AppStore()
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { },
        )

        store.dispatch(MenuAction.AddBookmark)
        store.waitUntilIdle()

        verify(addBookmarkUseCase).invoke(url = url, title = title, parentGuid = BookmarkRoot.Mobile.id)
    }

    @Test
    fun `GIVEN selected tab is bookmarked WHEN add bookmark action is dispatched THEN add bookmark use case is never called`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val guid = bookmarksStorage.addItem(
            parentGuid = BookmarkRoot.Mobile.id,
            url = url,
            title = title,
            position = 5u,
        )

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val captureMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
        val appStore = AppStore(middlewares = listOf(captureMiddleware))
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        // Wait for UpdateBookmarkState and middleware
        store.waitUntilIdle()

        assertEquals(guid, store.state.browserMenuState!!.bookmarkState.guid)
        assertTrue(store.state.browserMenuState!!.bookmarkState.isBookmarked)

        store.dispatch(MenuAction.AddBookmark)
        store.waitUntilIdle()

        verify(addBookmarkUseCase, never()).invoke(url = url, title = title)
        captureMiddleware.assertNotDispatched(BookmarkAction.BookmarkAdded::class)
        assertFalse(dismissWasCalled)
    }

    @Test
    fun `GIVEN selected tab is pinned WHEN init action is dispatched THEN initial pinned state is updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"

        whenever(pinnedSiteStorage.getPinnedSites()).thenReturn(
            listOf(
                TopSite.Pinned(
                    id = 0,
                    title = title,
                    url = url,
                    createdAt = 0,
                ),
            ),
        )

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        // Wait for UpdatePinnedState and middleware
        store.waitUntilIdle()

        assertTrue(store.state.browserMenuState!!.isPinned)
    }

    @Test
    fun `GIVEN selected tab is not pinned WHEN init action is dispatched THEN initial pinned state is not updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
        )

        assertFalse(store.state.browserMenuState!!.isPinned)

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        assertFalse(store.state.browserMenuState!!.isPinned)
    }

    @Test
    fun `WHEN add to shortcuts action is dispatched for a selected tab THEN the site is pinned`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissedWasCalled = false

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissedWasCalled = true },
        )

        store.dispatch(MenuAction.AddShortcut)
        store.waitUntilIdle()

        verify(addPinnedSiteUseCase).invoke(url = url, title = title)
        verify(appStore).dispatch(
            AppAction.ShortcutAction.ShortcutAdded,
        )
        assertTrue(dismissedWasCalled)
    }

    @Test
    fun `GIVEN selected tab is pinned WHEN add to shortcuts action is dispatched THEN add pinned site use case is never called`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissedWasCalled = false

        whenever(pinnedSiteStorage.getPinnedSites()).thenReturn(
            listOf(
                TopSite.Pinned(
                    id = 0,
                    title = title,
                    url = url,
                    createdAt = 0,
                ),
            ),
        )

        pinnedSiteStorage.addPinnedSite(
            url = url,
            title = title,
        )

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissedWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        // Wait for UpdatePinnedState and middleware
        store.waitUntilIdle()

        assertTrue(store.state.browserMenuState!!.isPinned)

        store.dispatch(MenuAction.AddShortcut)
        store.waitUntilIdle()

        verify(addPinnedSiteUseCase, never()).invoke(url = url, title = title)
        verify(appStore, never()).dispatch(
            AppAction.ShortcutAction.ShortcutAdded,
        )
        assertFalse(dismissedWasCalled)
    }

    @Test
    fun `WHEN remove from shortcuts action is dispatched for a selected tab THEN remove pinned site use case is never called`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissedWasCalled = false

        val topSite = TopSite.Pinned(
            id = 0,
            title = title,
            url = url,
            createdAt = 0,
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissedWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        assertFalse(store.state.browserMenuState!!.isPinned)

        store.dispatch(MenuAction.RemoveShortcut)
        store.waitUntilIdle()

        verify(removePinnedSiteUseCase, never()).invoke(topSite = topSite)
        verify(appStore, never()).dispatch(
            AppAction.ShortcutAction.ShortcutRemoved,
        )
        assertFalse(dismissedWasCalled)
    }

    @Test
    fun `GIVEN selected tab is pinned WHEN remove from shortcuts action is dispatched THEN pinned state is updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val topSite = TopSite.Pinned(
            id = 0,
            title = title,
            url = url,
            createdAt = 0,
        )
        var dismissedWasCalled = false

        whenever(pinnedSiteStorage.getPinnedSites()).thenReturn(listOf(topSite))

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissedWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        // Wait for UpdatePinnedState and middleware
        store.waitUntilIdle()

        assertTrue(store.state.browserMenuState!!.isPinned)

        store.dispatch(MenuAction.RemoveShortcut)
        store.waitUntilIdle()

        verify(removePinnedSiteUseCase).invoke(topSite = topSite)
        verify(appStore).dispatch(
            AppAction.ShortcutAction.ShortcutRemoved,
        )
        assertTrue(dismissedWasCalled)
    }

    @Test
    fun `GIVEN maximum number of top sites is reached WHEN add to shortcuts action is dispatched THEN add pinned site use case is never called`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissedWasCalled = false

        val pinnedSitesList = mutableListOf<TopSite>()

        repeat(TOP_SITES_MAX_COUNT) {
            pinnedSitesList.add(
                TopSite.Pinned(
                    id = 0,
                    title = title,
                    url = "$url/1",
                    createdAt = 0,
                ),
            )
        }

        whenever(pinnedSiteStorage.getPinnedSites()).thenReturn(pinnedSitesList)

        val newAlertDialog: AlertDialog = mock()
        whenever(alertDialogBuilder.create()).thenReturn(newAlertDialog)
        whenever(newAlertDialog.withCenterAlignedButtons()).thenReturn(null)

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissedWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        // Wait for UpdatePinnedState and middleware
        store.waitUntilIdle()

        assertFalse(store.state.browserMenuState!!.isPinned)

        store.dispatch(MenuAction.AddShortcut)
        store.waitUntilIdle()

        verify(addPinnedSiteUseCase, never()).invoke(url = url, title = title)
        verify(appStore, never()).dispatch(
            AppAction.ShortcutAction.ShortcutAdded,
        )
        assertTrue(dismissedWasCalled)
    }

    @Test
    fun `GIVEN selected tab has external app WHEN open in app action is dispatched THEN the site is opened in app`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        val getRedirect: AppLinksUseCases.GetAppLinkRedirect = mock()
        whenever(appLinksUseCases.appLinkRedirect).thenReturn(getRedirect)

        val redirect: AppLinkRedirect = mock()
        whenever(getRedirect.invoke(url)).thenReturn(redirect)
        whenever(redirect.hasExternalApp()).thenReturn(true)

        val intent: Intent = mock()
        whenever(redirect.appIntent).thenReturn(intent)

        val openAppLinkRedirect: AppLinksUseCases.OpenAppLinkRedirect = mock()
        whenever(appLinksUseCases.openAppLink).thenReturn(openAppLinkRedirect)

        store.dispatch(MenuAction.OpenInApp)
        store.waitUntilIdle()

        verify(openAppLinkRedirect).invoke(appIntent = intent)
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN selected tab does not have external app WHEN open in app action is dispatched THEN the site is not opened in app`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        val getRedirect: AppLinksUseCases.GetAppLinkRedirect = mock()
        whenever(appLinksUseCases.appLinkRedirect).thenReturn(getRedirect)

        val redirect: AppLinkRedirect = mock()
        whenever(getRedirect.invoke(url)).thenReturn(redirect)
        whenever(redirect.hasExternalApp()).thenReturn(false)

        val intent: Intent = mock()
        val openAppLinkRedirect: AppLinksUseCases.OpenAppLinkRedirect = mock()

        store.dispatch(MenuAction.OpenInApp)
        store.waitUntilIdle()

        verify(openAppLinkRedirect, never()).invoke(appIntent = intent)
        assertFalse(dismissWasCalled)
    }

    @Test
    fun `WHEN install addon action is dispatched THEN addon is installed`() = runTestOnMain {
        val addon = Addon(id = "ext1", downloadUrl = "downloadUrl")
        val store = createStore()

        store.dispatch(MenuAction.InstallAddon(addon))
        store.waitUntilIdle()

        verify(addonManager).installAddon(
            url = eq(addon.downloadUrl),
            installationMethod = eq(InstallationMethod.MANAGER),
            onSuccess = any(),
            onError = any(),
        )

        assertEquals(store.state.extensionMenuState.addonInstallationInProgress, addon)
    }

    @Test
    fun `GIVEN selected tab is readerable and reader view is off WHEN toggle reader view action is dispatched THEN reader view state is updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val readerState = ReaderState(
            readerable = true,
            active = false,
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
                readerState = readerState,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        store.dispatch(MenuAction.ToggleReaderView)
        store.waitUntilIdle()

        verify(appStore).dispatch(ReaderViewAction.ReaderViewStarted)
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN selected tab is readerable and reader view is on WHEN toggle reader view action is dispatched THEN reader view state is updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val readerState = ReaderState(
            readerable = true,
            active = true,
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
                readerState = readerState,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        store.dispatch(MenuAction.ToggleReaderView)
        store.waitUntilIdle()

        verify(appStore).dispatch(ReaderViewAction.ReaderViewDismissed)
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN selected tab is not readerable WHEN toggle reader view action is dispatched THEN reader view state is not updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val readerState = ReaderState(
            readerable = false,
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
                readerState = readerState,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        store.dispatch(MenuAction.ToggleReaderView)
        store.waitUntilIdle()

        verify(appStore, never()).dispatch(ReaderViewAction.ReaderViewStarted)
        assertFalse(dismissWasCalled)
    }

    @Test
    fun `WHEN customize reader view action is dispatched THEN reader view action is dispatched`() = runTestOnMain {
        var dismissWasCalled = false

        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(),
            onDismiss = { dismissWasCalled = true },
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        store.dispatch(MenuAction.CustomizeReaderView)
        store.waitUntilIdle()

        verify(appStore).dispatch(ReaderViewAction.ReaderViewControlsShown)
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `WHEN open in Firefox action is dispatched for a custom tab THEN the tab is opened in the browser`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissedWasCalled = false

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = spy(AppStore())
        val store = createStore(
            appStore = appStore,
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissedWasCalled = true },
        )

        store.dispatch(MenuAction.OpenInFirefox)
        store.waitUntilIdle()

        verify(appStore).dispatch(
            AppAction.OpenInFirefoxStarted,
        )
        assertTrue(dismissedWasCalled)
    }

    @Test
    fun `WHEN find in page action is dispatched THEN find in page app action is dispatched`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        var dismissWasCalled = false

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val appStore = spy(AppStore())
        val store = spy(
            createStore(
                appStore = appStore,
                menuState = MenuState(
                    browserMenuState = browserMenuState,
                ),
                onDismiss = { dismissWasCalled = true },
            ),
        )

        store.waitUntilIdle()

        store.dispatch(MenuAction.FindInPage)
        store.waitUntilIdle()

        verify(appStore).dispatch(FindInPageAction.FindInPageStarted)
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `WHEN custom menu item action is dispatched THEN pending intent is sent with url`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val mockIntent: PendingIntent = mock()
        var dismissWasCalled = false
        var sentIntent: PendingIntent? = null
        var sentUrl: String? = null

        val store = spy(
            createStore(
                onDismiss = { dismissWasCalled = true },
                onSendPendingIntentWithUrl = { _, _ ->
                    sentIntent = mockIntent
                    sentUrl = url
                },
            ),
        )
        store.waitUntilIdle()

        assertNull(sentIntent)
        assertNull(sentUrl)

        store.dispatch(
            MenuAction.CustomMenuItemAction(
                intent = mockIntent,
                url = url,
            ),
        )
        store.waitUntilIdle()

        assertEquals(sentIntent, mockIntent)
        assertEquals(sentUrl, url)
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN menu is accessed from the browser WHEN request desktop mode action is dispatched THEN request desktop site use case is invoked`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val selectedTab = createTab(
            url = url,
            title = title,
            desktopMode = false,
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = selectedTab,
        )
        var dismissWasCalled = false
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.RequestDesktopSite)
        store.waitUntilIdle()

        verify(requestDesktopSiteUseCase).invoke(
            enable = eq(true),
            tabId = eq(selectedTab.id),
        )
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `GIVEN menu is accessed from the browser and desktop mode is enabled WHEN request mobile mode action is dispatched THEN request desktop site use case is invoked`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val isDesktopMode = true
        val selectedTab = createTab(
            url = url,
            title = title,
            desktopMode = isDesktopMode,
        )
        val browserMenuState = BrowserMenuState(
            selectedTab = selectedTab,
        )
        var dismissWasCalled = false
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
                isDesktopMode = isDesktopMode,
            ),
            onDismiss = { dismissWasCalled = true },
        )

        store.dispatch(MenuAction.RequestMobileSite)
        store.waitUntilIdle()

        verify(requestDesktopSiteUseCase).invoke(
            enable = eq(false),
            tabId = eq(selectedTab.id),
        )
        assertTrue(dismissWasCalled)
    }

    private fun createStore(
        appStore: AppStore = AppStore(),
        menuState: MenuState = MenuState(),
        onDismiss: suspend () -> Unit = {},
        onSendPendingIntentWithUrl: (intent: PendingIntent, url: String?) -> Unit = { _: PendingIntent, _: String? -> },
    ) = MenuStore(
        initialState = menuState,
        middleware = listOf(
            MenuDialogMiddleware(
                appStore = appStore,
                addonManager = addonManager,
                settings = settings,
                bookmarksStorage = bookmarksStorage,
                pinnedSiteStorage = pinnedSiteStorage,
                appLinksUseCases = appLinksUseCases,
                addBookmarkUseCase = addBookmarkUseCase,
                addPinnedSiteUseCase = addPinnedSiteUseCase,
                removePinnedSitesUseCase = removePinnedSiteUseCase,
                requestDesktopSiteUseCase = requestDesktopSiteUseCase,
                alertDialogBuilder = alertDialogBuilder,
                topSitesMaxLimit = TOP_SITES_MAX_COUNT,
                onDeleteAndQuit = onDeleteAndQuit,
                onDismiss = onDismiss,
                onSendPendingIntentWithUrl = onSendPendingIntentWithUrl,
                scope = scope,
            ),
        ),
    )
}
