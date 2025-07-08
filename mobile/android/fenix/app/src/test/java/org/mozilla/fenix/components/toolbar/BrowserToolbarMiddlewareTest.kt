/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.os.Looper
import androidx.fragment.app.FragmentManager
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction.UpdateLoadingStateAction
import mozilla.components.browser.state.action.ContentAction.UpdateProgressAction
import mozilla.components.browser.state.action.ContentAction.UpdateSecurityInfoAction
import mozilla.components.browser.state.action.ContentAction.UpdateUrlAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction.AddTabAction
import mozilla.components.browser.state.action.TabListAction.RemoveTabAction
import mozilla.components.browser.state.ext.getUrl
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButtonRes
import mozilla.components.compose.browser.toolbar.concept.Action.TabCounterAction
import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.CopyToClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.LoadFromClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.PasteFromClipboardClicked
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.CombinedEventAndMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription.StringResContentDescription
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon.DrawableResIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text.StringResText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuDivider
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.EnvironmentCleared
import mozilla.components.compose.browser.toolbar.store.EnvironmentRehydrated
import mozilla.components.compose.browser.toolbar.store.ProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity.Bottom
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity.Top
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.lib.state.Middleware
import mozilla.components.support.ktx.util.URLStringUtils
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.utils.ClipboardHandler
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.NimbusEventStore
import org.mozilla.fenix.GleanMetrics.AddressToolbar
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.ReaderMode
import org.mozilla.fenix.GleanMetrics.Translations
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.PageTranslationStatus
import org.mozilla.fenix.browser.ReaderModeStatus
import org.mozilla.fenix.browser.browsingmode.BrowsingMode.Normal
import org.mozilla.fenix.browser.browsingmode.BrowsingMode.Private
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.SimpleBrowsingModeManager
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.browser.store.BrowserScreenAction
import org.mozilla.fenix.browser.store.BrowserScreenAction.ClosingLastPrivateTab
import org.mozilla.fenix.browser.store.BrowserScreenAction.PageTranslationStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenAction.ReaderModeStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenState
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.browser.store.BrowserScreenStore.Environment
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.NimbusComponents
import org.mozilla.fenix.components.UseCases
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.CurrentTabClosed
import org.mozilla.fenix.components.appstate.AppAction.SnackbarAction.SnackbarDismissed
import org.mozilla.fenix.components.appstate.AppAction.URLCopiedToClipboard
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.OrientationMode.Landscape
import org.mozilla.fenix.components.appstate.OrientationMode.Portrait
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.BrowserToolbarMiddleware.ToolbarAction
import org.mozilla.fenix.components.toolbar.DisplayActions.AddBookmarkClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.EditBookmarkClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.MenuClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.NavigateBackClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.NavigateBackLongClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.NavigateForwardClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.NavigateForwardLongClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.RefreshClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.StopRefreshClicked
import org.mozilla.fenix.components.toolbar.PageEndActionsInteractions.ReaderModeClicked
import org.mozilla.fenix.components.toolbar.PageEndActionsInteractions.TranslateClicked
import org.mozilla.fenix.components.toolbar.PageOriginInteractions.OriginClicked
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.AddNewPrivateTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.AddNewTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.CloseCurrentTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.TabCounterClicked
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.TabCounterLongClicked
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases.Companion.ABOUT_HOME
import org.mozilla.fenix.ext.isLargeWindow
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.tabstray.TabManagementFeatureHelper
import org.mozilla.fenix.utils.Settings
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import mozilla.components.ui.icons.R as iconsR

@RunWith(AndroidJUnit4::class)
class BrowserToolbarMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanRule = FenixGleanTestRule(testContext)

    private val appStore: AppStore = mockk(relaxed = true)
    private val browserScreenState: BrowserScreenState = mockk(relaxed = true)
    private val browserScreenStore: BrowserScreenStore = mockk(relaxed = true) {
        every { state } returns browserScreenState
    }
    private val browserStore = BrowserStore()
    private val clipboard: ClipboardHandler = mockk(relaxed = true)
    private val lifecycleOwner = FakeLifecycleOwner(Lifecycle.State.RESUMED)
    private val navController: NavController = mockk(relaxed = true)
    private val browsingModeManager = SimpleBrowsingModeManager(Normal)
    private val browserAnimator: BrowserAnimator = mockk(relaxed = true)
    private val thumbnailsFeature: BrowserThumbnails = mockk(relaxed = true)
    private val readerModeController: ReaderModeController = mockk(relaxed = true)
    private val useCases: UseCases = mockk(relaxed = true)
    val nimbusEventsStore: NimbusEventStore = mockk {
        every { recordEvent(any()) } just Runs
    }
    private val nimbusComponents: NimbusComponents = mockk {
        every { events } returns nimbusEventsStore
    }
    private val settings: Settings = mockk(relaxed = true) {
        every { shouldUseBottomToolbar } returns true
        every { shouldUseSimpleToolbar } returns true
    }
    private val tabId = "test"
    private val tab: TabSessionState = mockk(relaxed = true) {
        every { id } returns tabId
    }
    private val permissionsStorage: SitePermissionsStorage = mockk()
    private val cookieBannersStorage: CookieBannersStorage = mockk()
    private val trackingProtectionUseCases: TrackingProtectionUseCases = mockk()
    private val publicSuffixList = PublicSuffixList(testContext)
    private val bookmarksStorage: BookmarksStorage = mockk(relaxed = true)

    @Test
    fun `WHEN initializing the toolbar THEN update state to display mode`() = runTestOnMain {
        val appStore: AppStore = mockk(relaxed = true)
        val middleware = buildMiddleware(appStore = appStore)

        val toolbarStore = buildStore(middleware)

        verify { appStore.dispatch(UpdateSearchBeingActiveState(false)) }
    }

    @Test
    fun `WHEN initializing the toolbar THEN add browser start actions`() = runTestOnMain {
        val toolbarStore = buildStore()

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsStart
        assertEquals(emptyList<Action>(), toolbarBrowserActions)
    }

    @Test
    fun `WHEN initializing the toolbar THEN add browser end actions`() = runTestOnMain {
        val toolbarStore = buildStore()

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
        val newTabButton = toolbarBrowserActions[0]
        val tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        val menuButton = toolbarBrowserActions[2]
        assertEquals(expectedNewTabButton, newTabButton)
        assertEqualsTabCounterButton(expectedTabCounterButton(), tabCounterButton)
        assertEquals(expectedMenuButton, menuButton)
    }

    @Test
    fun `GIVEN normal browsing mode WHEN initializing the toolbar THEN show the number of normal tabs in the tabs counter button`() = runTestOnMain {
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(createTab("test.com", private = false)),
            ),
        )
        val middleware = buildMiddleware(browserStore = browserStore)

        val toolbarStore = buildStore(
            middleware = middleware,
            browsingModeManager = browsingModeManager,
        )

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        val tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1), tabCounterButton)
    }

    @Test
    fun `GIVEN private browsing mode WHEN initializing the toolbar THEN show the number of private tabs in the tabs counter button`() = runTestOnMain {
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("test.com", private = true),
                    createTab("firefox.com", private = true),
                ),
            ),
        )
        val middleware = buildMiddleware(browserStore = browserStore)

        val toolbarStore = buildStore(
            middleware = middleware,
            browsingModeManager = browsingModeManager,
        )

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        val tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(2, true), tabCounterButton)
    }

    @Test
    fun `WHEN initializing the toolbar THEN setup showing the website origin`() {
        val initialTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(initialTab),
                selectedTabId = initialTab.id,
            ),
        )
        val expectedConfiguration = PageOrigin(
            hint = R.string.search_hint,
            title = null,
            url = initialTab.getUrl(),
            contextualMenuOptions = ContextualMenuOption.entries,
            onClick = OriginClicked,
        )
        val middleware = buildMiddleware(browserStore = browserStore)

        val toolbarStore = buildStore(middleware)

        val originConfiguration = toolbarStore.state.displayState.pageOrigin
        assertEqualsOrigin(expectedConfiguration, originConfiguration)
    }

    @Test
    fun `GIVEN an environment was already set WHEN it is cleared THEN reset it to null`() {
        val middleware = buildMiddleware()
        val store = buildStore(middleware)

        assertNotNull(middleware.environment)

        store.dispatch(EnvironmentCleared)

        assertNull(middleware.environment)
    }

    @Test
    fun `GIVEN ABOUT_HOME URL WHEN the page origin is modified THEN update the page origin`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())

        val tab = createTab("https://mozilla.com/")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            ),
        )
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = buildStore(middleware)
        testScheduler.advanceUntilIdle()

        val pageOrigin = PageOrigin(
            hint = R.string.search_hint,
            title = null,
            url = URLStringUtils.toDisplayUrl(tab.getUrl()!!).toString(),
            contextualMenuOptions = ContextualMenuOption.entries,
            onClick = OriginClicked,
        )
        assertEqualsOrigin(pageOrigin, toolbarStore.state.displayState.pageOrigin)

        browserStore.dispatch(UpdateUrlAction(sessionId = tab.id, url = ABOUT_HOME)).joinBlocking()
        testScheduler.advanceUntilIdle()

        assertEqualsOrigin(
            pageOrigin.copy(
                url = "",
            ),
            toolbarStore.state.displayState.pageOrigin,
        )
    }

    @Test
    fun `GIVEN in portrait WHEN changing to landscape THEN keep browser end actions`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        // In portrait the navigation bar is displayed
        val appStore = AppStore(
            initialState = AppState(
                orientation = Portrait,
            ),
        )
        val middleware = buildMiddleware(appStore = appStore)
        val toolbarStore = buildStore(middleware)
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)

        appStore.dispatch(AppAction.OrientationChange(Landscape)).joinBlocking()
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
        val newTabButton = toolbarBrowserActions[0]
        val tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        val menuButton = toolbarBrowserActions[2]
        assertEquals(expectedNewTabButton, newTabButton)
        assertEqualsTabCounterButton(expectedTabCounterButton(), tabCounterButton)
        assertEquals(expectedMenuButton, menuButton)
    }

    @Test
    fun `GIVEN in landscape WHEN changing to portrait THEN keep all browser end actions`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        // In landscape the navigation bar is not displayed
        val appStore = AppStore(
            initialState = AppState(
                orientation = Landscape,
            ),
        )
        val middleware = buildMiddleware(appStore = appStore)

        val toolbarStore = buildStore(middleware)
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
        val newTabButton = toolbarBrowserActions[0] as ActionButtonRes
        val tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        val menuButton = toolbarBrowserActions[2] as ActionButtonRes
        assertEquals(expectedNewTabButton, newTabButton)
        assertEqualsTabCounterButton(expectedTabCounterButton(), tabCounterButton)
        assertEquals(expectedMenuButton, menuButton)

        // In portrait the navigation bar is displayed
        appStore.dispatch(AppAction.OrientationChange(Portrait)).joinBlocking()
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
    }

    @Test
    fun `GIVEN in normal browsing WHEN the number of normal opened tabs is modified THEN update the tab counter`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val browserStore = BrowserStore()
        val middleware = buildMiddleware(browserStore = browserStore)

        val toolbarStore = buildStore(
            middleware = middleware,
            browsingModeManager = browsingModeManager,
        )
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
        var tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0), tabCounterButton)

        val newNormalTab = createTab("test.com", private = false)
        val newPrivateTab = createTab("test.com", private = true)
        browserStore.dispatch(AddTabAction(newNormalTab)).joinBlocking()
        browserStore.dispatch(AddTabAction(newPrivateTab)).joinBlocking()
        shadowOf(Looper.getMainLooper()).idle() // wait for observing and processing the search engine update
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
        tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1), tabCounterButton)
    }

    @Test
    fun `GIVEN in private browsing WHEN the number of private opened tabs is modified THEN update the tab counter`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val initialNormalTab = createTab("test.com", private = false)
        val initialPrivateTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(initialNormalTab, initialPrivateTab),
            ),
        )
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = buildStore(
            middleware = middleware,
            browsingModeManager = browsingModeManager,
        )
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
        var tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)

        browserStore.dispatch(RemoveTabAction(initialPrivateTab.id)).joinBlocking()
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(3, toolbarBrowserActions.size)
        tabCounterButton = toolbarBrowserActions[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0, true), tabCounterButton)
    }

    @Test
    fun `WHEN clicking the new tab button THEN navigate to application's home screen`() {
        val browserAnimatorActionCaptor = slot<(Boolean) -> Unit>()
        every {
            browserAnimator.captureEngineViewAndDrawStatically(
                any<Long>(),
                capture(browserAnimatorActionCaptor),
            )
        } answers { browserAnimatorActionCaptor.captured.invoke(true) }
        val middleware = buildMiddleware()
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
        )
        val newTabButton = toolbarStore.state.displayState.browserActionsEnd[0] as ActionButtonRes

        mockkStatic(NavController::nav) {
            toolbarStore.dispatch(newTabButton.onClick as BrowserToolbarEvent)

            verify { navController.navigate(BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true)) }
        }
    }

    @Test
    fun `WHEN clicking the menu button THEN open the menu`() {
        val middleware = buildMiddleware()
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
        )
        val menuButton = toolbarStore.state.displayState.browserActionsEnd[2] as ActionButtonRes

        mockkStatic(NavController::nav) {
            toolbarStore.dispatch(menuButton.onClick as BrowserToolbarEvent)

            verify {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalMenuDialogFragment(
                        accesspoint = MenuAccessPoint.Browser,
                    ),
                )
            }
        }
    }

    @Test
    fun `GIVEN browsing in normal mode WHEN clicking the tab counter button THEN open the tabs tray in normal mode`() {
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val thumbnailsFeature: BrowserThumbnails = mockk(relaxed = true)
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
            thumbnailsFeature = thumbnailsFeature,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction

        mockkStatic(NavController::nav) {
            toolbarStore.dispatch(tabCounterButton.onClick)

            verify {
                navController.nav(
                    R.id.browserFragment,
                    NavGraphDirections.actionGlobalTabsTrayFragment(page = Page.NormalTabs),
                )
            }
            verify {
                thumbnailsFeature.requestScreenshot()
            }
        }
        assertEquals("tabs_tray", Events.browserToolbarAction.testGetValue()?.last()?.extra?.get("item"))
    }

    @Test
    fun `GIVEN browsing in private mode WHEN clicking the tab counter button THEN open the tabs tray in private mode`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val thumbnailsFeature: BrowserThumbnails = mockk(relaxed = true)
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
            thumbnailsFeature = thumbnailsFeature,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction

        mockkStatic(NavController::nav) {
            toolbarStore.dispatch(tabCounterButton.onClick)

            verify {
                navController.nav(
                    R.id.browserFragment,
                    NavGraphDirections.actionGlobalTabsTrayFragment(page = Page.PrivateTabs),
                )
            }
            verify {
                thumbnailsFeature.requestScreenshot()
            }
        }
        assertEquals("tabs_tray", Events.browserToolbarAction.testGetValue()?.last()?.extra?.get("item"))
    }

    @Test
    fun `WHEN long clicking the tab counter button THEN record telemetry`() {
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = buildStore(middleware)
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction

        toolbarStore.dispatch((tabCounterButton.onLongClick as CombinedEventAndMenu).event)

        assertEquals("tabs_tray_long_press", Events.browserToolbarAction.testGetValue()?.last()?.extra?.get("item"))
    }

    @Test
    fun `WHEN clicking on the first option in the toolbar long click menu THEN open a new normal tab`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0, false), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as CombinedEventAndMenu).menu.items()

        toolbarStore.dispatch((tabCounterMenuItems[0] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Normal, browsingModeManager.mode)
        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
            )
        }
    }

    @Test
    fun `GIVEN no search terms for the current tab WHEN the page origin is clicked THEN start search in the home screen`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val currentTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )

        toolbarStore.dispatch(toolbarStore.state.displayState.pageOrigin.onClick as BrowserToolbarAction)

        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(
                    focusOnAddressBar = true,
                    sessionToStartSearchFor = browserStore.state.selectedTabId,
                ),
            )
        }
    }

    @Test
    fun `GIVEN in the browser sceen WHEN clicking on the URL THEN record telemetry`() {
        every { navController.currentDestination?.id } returns R.id.browserFragment
        val middleware = buildMiddleware()
        val toolbarStore = buildStore(middleware, navController = navController)

        toolbarStore.dispatch(toolbarStore.state.displayState.pageOrigin.onClick as BrowserToolbarAction)

        assertEquals("BROWSER", Events.searchBarTapped.testGetValue()?.last()?.extra?.get("source"))
    }

    @Test
    fun `GIVEN in the home sceen WHEN clicking on the URL THEN record telemetry`() {
        every { navController.currentDestination?.id } returns R.id.homeFragment
        val middleware = buildMiddleware()
        val toolbarStore = buildStore(middleware, navController = navController)

        toolbarStore.dispatch(toolbarStore.state.displayState.pageOrigin.onClick as BrowserToolbarAction)

        assertEquals("HOME", Events.searchBarTapped.testGetValue()?.last()?.extra?.get("source"))
    }

    @Test
    @Config(sdk = [30])
    fun `GIVEN on Android 11 WHEN choosing to copy the current URL to clipboard THEN copy to clipboard and show a snackbar`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val clipboard = ClipboardHandler(testContext)
        val currentTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val appStore: AppStore = mockk(relaxed = true)
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            clipboard = clipboard,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )

        toolbarStore.dispatch(CopyToClipboardClicked)

        assertEquals(currentTab.getUrl(), clipboard.text)
        verify { appStore.dispatch(URLCopiedToClipboard) }
        assertNotNull(Events.copyUrlTapped.testGetValue())
    }

    @Test
    @Config(sdk = [31])
    fun `GIVEN on Android 12 WHEN choosing to copy the current URL to clipboard THEN copy to clipboard and show a snackbar`() {
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val navController: NavController = mockk(relaxed = true)
        val clipboard = ClipboardHandler(testContext)
        val currentTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val appStore: AppStore = mockk(relaxed = true)
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            clipboard = clipboard,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )

        toolbarStore.dispatch(CopyToClipboardClicked)

        assertEquals(currentTab.getUrl(), clipboard.text)
        verify { appStore.dispatch(URLCopiedToClipboard) }
        assertNotNull(Events.copyUrlTapped.testGetValue())
    }

    @Test
    @Config(sdk = [33])
    fun `GIVEN on Android 13 WHEN choosing to copy the current URL to clipboard THEN copy to clipboard and don't show a snackbar`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val clipboard = ClipboardHandler(testContext)
        val currentTab = createTab("firefox.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val appStore: AppStore = mockk(relaxed = true)
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            clipboard = clipboard,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )

        toolbarStore.dispatch(CopyToClipboardClicked)

        assertEquals(currentTab.getUrl(), clipboard.text)
        verify(exactly = 0) { appStore.dispatch(URLCopiedToClipboard) }
        assertNotNull(Events.copyUrlTapped.testGetValue())
    }

    @Test
    fun `WHEN choosing to paste from clipboard THEN start a new search with the current clipboard text`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val clipboard = ClipboardHandler(testContext).also {
            it.text = "test"
        }
        val currentTab = createTab("firefox.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val middleware = buildMiddleware(
            browserStore = browserStore,
            clipboard = clipboard,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )

        mockkStatic(Context::settings) {
            mockkStatic(NavController::nav) {
                every { testContext.settings().toolbarPosition } returns ToolbarPosition.TOP

                toolbarStore.dispatch(PasteFromClipboardClicked)

                verify {
                    navController.nav(
                        R.id.browserFragment,
                        BrowserFragmentDirections.actionGlobalSearchDialog(
                            sessionId = currentTab.id,
                            pastedText = "test",
                        ),
                        NavOptions.Builder()
                            .setEnterAnim(R.anim.fade_in)
                            .setExitAnim(R.anim.fade_out)
                            .build(),
                    )
                }
            }
        }
    }

    @Test
    fun `WHEN choosing to load URL from clipboard THEN start load the URL from clipboard in a new tab`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val clipboardUrl = "https://www.mozilla.com"
        val clipboard = ClipboardHandler(testContext).also {
            it.text = clipboardUrl
        }
        val browserUseCases: FenixBrowserUseCases = mockk(relaxed = true)
        val useCases: UseCases = mockk {
            every { fenixBrowserUseCases } returns browserUseCases
        }
        val middleware = buildMiddleware(
            browserStore = browserStore,
            useCases = useCases,
            clipboard = clipboard,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )

        mockkStatic(Context::settings) {
            mockkStatic(NavController::nav) {
                every { testContext.settings().toolbarPosition } returns ToolbarPosition.TOP

                toolbarStore.dispatch(LoadFromClipboardClicked)

                verify {
                    browserUseCases.loadUrlOrSearch(
                        searchTermOrURL = clipboardUrl,
                        newTab = false,
                        private = false,
                    )
                }
            }
        }
        assertEquals(
            "false", Events.enteredUrl.testGetValue()?.last()?.extra?.get("autocomplete"),
        )
    }

    @Test
    fun `WHEN clicking on the second option in the toolbar long click menu THEN open a new private tab`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val middleware = buildMiddleware()
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0, false), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as CombinedEventAndMenu).menu.items()

        toolbarStore.dispatch((tabCounterMenuItems[1] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
            )
        }
    }

    @Test
    fun `GIVEN multiple tabs opened WHEN clicking on the close tab item in the tab counter long click menu THEN close the current tab`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val appStore: AppStore = mockk(relaxed = true)
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com", private = true)),
                selectedTabId = currentTab.id,
            ),
        )
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        every { useCases.tabsUseCases } returns tabsUseCases
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            useCases = useCases,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(2, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as CombinedEventAndMenu).menu.items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify {
            tabsUseCases.removeTab(currentTab.id, true)
            appStore.dispatch(CurrentTabClosed(true))
        }
        verify(exactly = 0) {
            navController.navigate(any<NavDirections>())
        }
    }

    @Test
    fun `GIVEN on the last open normal tab WHEN clicking on the close tab item in the tab counter long click menu THEN navigate to home before closing the tab`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val appStore: AppStore = mockk(relaxed = true)
        val currentTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            useCases = useCases,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, false), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as CombinedEventAndMenu).menu.items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Normal, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(CurrentTabClosed(true))
            appStore.dispatch(CurrentTabClosed(false))
        }
        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(
                    sessionToDelete = currentTab.id,
                ),
            )
        }
    }

    @Test
    fun `GIVEN on the last open private tab and no private downloads WHEN clicking on the close tab item THEN navigate to home before closing the tab`() {
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
            ),
        )
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            useCases = useCases,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as CombinedEventAndMenu).menu.items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(CurrentTabClosed(true))
            appStore.dispatch(CurrentTabClosed(false))
        }
        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(
                    sessionToDelete = currentTab.id,
                ),
            )
        }
    }

    @Test
    fun `GIVEN on the last open private tab with private downloads in progress WHEN clicking on the close tab item THEN navigate to home before closing the tab`() {
        every { browserScreenStore.state } returns BrowserScreenState(
            cancelPrivateDownloadsAccepted = false,
        )
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
                downloads = mapOf("test" to DownloadState("download", private = true)),
            ),
        )
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            useCases = useCases,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as CombinedEventAndMenu).menu.items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(CurrentTabClosed(true))
            appStore.dispatch(CurrentTabClosed(false))
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(
                    sessionToDelete = currentTab.id,
                ),
            )
        }
        verify {
            browserScreenStore.dispatch(
                ClosingLastPrivateTab(
                    tabId = currentTab.id,
                    inProgressPrivateDownloads = 1,
                ),
            )
        }
    }

    @Test
    fun `GIVEN on the last open private tab and accepted cancelling private downloads WHEN clicking on the close tab item THEN inform about closing the last private tab`() {
        every { browserScreenStore.state } returns BrowserScreenState(
            cancelPrivateDownloadsAccepted = true,
        )
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
                downloads = mapOf("test" to DownloadState("download", private = true)),
            ),
        )
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        val middleware = buildMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            useCases = useCases,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[1] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as CombinedEventAndMenu).menu.items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(CurrentTabClosed(true))
            appStore.dispatch(CurrentTabClosed(false))
            browserScreenStore.dispatch(any())
        }
        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(
                    sessionToDelete = currentTab.id,
                ),
            )
        }
    }

    @Test
    fun `GIVEN a bottom toolbar WHEN the loading progress of the current tab changes THEN update the progress bar`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        every { settings.shouldUseBottomToolbar } returns true
        val currentTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val middleware = buildMiddleware(
            browserStore = browserStore,
            useCases = useCases,
        )
        val toolbarStore = buildStore(middleware).also {
            it.dispatch(BrowserToolbarAction.Init())
        }
        testScheduler.advanceUntilIdle()

        browserStore.dispatch(UpdateProgressAction(currentTab.id, 50)).joinBlocking()
        testScheduler.advanceUntilIdle()

        assertEquals(
            ProgressBarConfig(
                progress = 50,
                gravity = Top,
                color = null,
            ),
            toolbarStore.state.displayState.progressBarConfig,
        )
    }

    @Test
    fun `GIVEN a top toolbar WHEN the loading progress of the current tab changes THEN update the progress bar`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        every { settings.shouldUseBottomToolbar } returns false
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab),
                selectedTabId = currentTab.id,
            ),
        )
        val middleware = buildMiddleware(
            browserStore = browserStore,
            useCases = useCases,
        )
        val toolbarStore = buildStore(middleware).also {
            it.dispatch(BrowserToolbarAction.Init())
        }
        testScheduler.advanceUntilIdle()

        browserStore.dispatch(UpdateProgressAction(currentTab.id, 71)).joinBlocking()
        testScheduler.advanceUntilIdle()

        assertEquals(
            ProgressBarConfig(
                progress = 71,
                gravity = Bottom,
                color = null,
            ),
            toolbarStore.state.displayState.progressBarConfig,
        )
    }

    @Test
    fun `GIVEN the current page can be viewed in reader mode WHEN tapping on the reader mode button THEN show the reader mode UX`() {
        val currentTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
            ),
        )
        val browserScreenStore = buildBrowserScreenStore()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val middleware = buildMiddleware(
            browserScreenStore = browserScreenStore,
            browserStore = browserStore,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            readerModeController = readerModeController,
        )

        browserScreenStore.dispatch(
            ReaderModeStatusUpdated(
                ReaderModeStatus(
                    isAvailable = true,
                    isActive = false,
                ),
            ),
        )

        val readerModeButton = toolbarStore.state.displayState.pageActionsEnd[0] as ActionButtonRes
        assertEquals(expectedReaderModeButton(false), readerModeButton)

        toolbarStore.dispatch(readerModeButton.onClick as BrowserToolbarEvent)
        verify { readerModeController.showReaderView() }
        assertNotNull(ReaderMode.opened.testGetValue())
    }

    @Test
    fun `GIVEN the current page is already viewed in reader mode WHEN tapping on the reader mode button THEN close the reader mode UX`() {
        val currentTab = createTab("test.com")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
            ),
        )
        val browserScreenStore = buildBrowserScreenStore()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val middleware = buildMiddleware(
            browserScreenStore = browserScreenStore,
            browserStore = browserStore,
        )
        val toolbarStore = buildStore(
            middleware = middleware,
            readerModeController = readerModeController,
        )

        browserScreenStore.dispatch(
            ReaderModeStatusUpdated(
                ReaderModeStatus(
                    isAvailable = true,
                    isActive = true,
                ),
            ),
        )

        val readerModeButton = toolbarStore.state.displayState.pageActionsEnd[0] as ActionButtonRes
        assertEquals(expectedReaderModeButton(true), readerModeButton)

        toolbarStore.dispatch(readerModeButton.onClick as BrowserToolbarEvent)
        verify { readerModeController.hideReaderView() }
        assertNotNull(ReaderMode.closed.testGetValue())
    }

    @Test
    fun `WHEN translation is possible THEN show a translate button`() {
        val browserScreenStore = buildBrowserScreenStore()
        val middleware = buildMiddleware(appStore, browserScreenStore, browserStore)
        val toolbarStore = buildStore(middleware, browsingModeManager = browsingModeManager, navController = navController)

        browserScreenStore.dispatch(
            PageTranslationStatusUpdated(
                PageTranslationStatus(
                    isTranslationPossible = true,
                    isTranslated = false,
                    isTranslateProcessing = false,
                ),
            ),
        )

        val translateButton = toolbarStore.state.displayState.pageActionsEnd[0]
        assertEquals(expectedTranslateButton, translateButton)
    }

    @Test
    fun `GIVEN the current page is translated WHEN knowing of this state THEN update the translate button to show this`() {
        val browserScreenStore = buildBrowserScreenStore()
        val middleware = buildMiddleware(appStore, browserScreenStore, browserStore)
        val toolbarStore = buildStore(middleware, browsingModeManager = browsingModeManager, navController = navController)

        browserScreenStore.dispatch(
            PageTranslationStatusUpdated(
                PageTranslationStatus(
                    isTranslationPossible = true,
                    isTranslated = false,
                    isTranslateProcessing = false,
                ),
            ),
        )
        var translateButton = toolbarStore.state.displayState.pageActionsEnd[0]
        assertEquals(expectedTranslateButton, translateButton)

        browserScreenStore.dispatch(
            PageTranslationStatusUpdated(
                PageTranslationStatus(
                    isTranslationPossible = true,
                    isTranslated = true,
                    isTranslateProcessing = false,
                ),
            ),
        )
        translateButton = toolbarStore.state.displayState.pageActionsEnd[0]
        assertEquals(
            expectedTranslateButton.copy(state = ActionButton.State.ACTIVE),
            translateButton,
        )
    }

    @Test
    fun `GIVEN translation is possible WHEN tapping on the translate button THEN allow user to choose how to translate`() {
        val currentNavDestination: NavDestination = mockk {
            every { id } returns R.id.browserFragment
        }
        val navController: NavController = mockk(relaxed = true) {
            every { currentDestination } returns currentNavDestination
        }

        val browserScreenStore = buildBrowserScreenStore()
        val middleware = buildMiddleware(appStore, browserScreenStore, browserStore)
        val toolbarStore = buildStore(middleware, browsingModeManager = browsingModeManager, navController = navController)
        browserScreenStore.dispatch(
            PageTranslationStatusUpdated(
                PageTranslationStatus(
                    isTranslationPossible = true,
                    isTranslated = false,
                    isTranslateProcessing = false,
                ),
            ),
        )

        val translateButton = toolbarStore.state.displayState.pageActionsEnd[0] as ActionButtonRes
        toolbarStore.dispatch(translateButton.onClick as BrowserToolbarEvent)

        verify { appStore.dispatch(SnackbarDismissed) }
        verify { navController.navigate(BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment()) }
        assertEquals("main_flow_toolbar", Translations.action.testGetValue()?.last()?.extra?.get("item"))
    }

    @Test
    fun `GIVEN device has large window WHEN a website is loaded THEN show navigation buttons`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        mockkStatic(Context::isLargeWindow) {
            every { any<Context>().isLargeWindow() } returns true
            every { settings.shouldUseBottomToolbar } returns false
            val middleware = buildMiddleware()
            val toolbarStore = buildStore(middleware)
            testScheduler.advanceUntilIdle()

            val displayGoBackButton = toolbarStore.state.displayState.browserActionsStart[0]
            assertEquals(displayGoBackButton, expectedGoBackButton.copy(state = ActionButton.State.DISABLED))
            val displayGoForwardButton = toolbarStore.state.displayState.browserActionsStart[1]
            assertEquals(displayGoForwardButton, expectedGoForwardButton.copy(state = ActionButton.State.DISABLED))
        }
    }

    @Test
    fun `GIVEN nav buttons on toolbar are shown WHEN device is rotated THEN nav buttons still shown`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        mockkStatic(Context::isLargeWindow) {
            every { any<Context>().isLargeWindow() } returns true
            every { settings.shouldUseBottomToolbar } returns false
            val middleware = buildMiddleware(appStore)
            val toolbarStore = buildStore(middleware)
            testScheduler.advanceUntilIdle()

            var displayGoBackButton = toolbarStore.state.displayState.browserActionsStart[0]
            assertEquals(displayGoBackButton, expectedGoBackButton.copy(state = ActionButton.State.DISABLED))
            var displayGoForwardButton = toolbarStore.state.displayState.browserActionsStart[1]
            assertEquals(displayGoForwardButton, expectedGoForwardButton.copy(state = ActionButton.State.DISABLED))

            appStore.dispatch(AppAction.OrientationChange(Landscape)).joinBlocking()
            testScheduler.advanceUntilIdle()

            displayGoBackButton = toolbarStore.state.displayState.browserActionsStart[0]
            assertEquals(displayGoBackButton, expectedGoBackButton.copy(state = ActionButton.State.DISABLED))
            displayGoForwardButton = toolbarStore.state.displayState.browserActionsStart[1]
            assertEquals(displayGoForwardButton, expectedGoForwardButton.copy(state = ActionButton.State.DISABLED))
        }
    }

    @Test
    fun `GIVEN the back button is shown WHEN interacted with THEN go back or show history and record telemetry`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        mockkStatic(Context::isLargeWindow) {
            every { any<Context>().isLargeWindow() } returns true
            every { settings.shouldUseBottomToolbar } returns false
            val currentTab = createTab("test.com", private = false)
            val captureMiddleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
            val browserStore = BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(currentTab),
                    selectedTabId = currentTab.id,
                ),
                middleware = listOf(captureMiddleware),
            )
            val middleware = buildMiddleware(appStore, browserStore = browserStore)
            val toolbarStore = buildStore(middleware)
            testScheduler.advanceUntilIdle()

            val backButton = toolbarStore.state.displayState.browserActionsStart[0] as ActionButtonRes
            toolbarStore.dispatch(backButton.onClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            captureMiddleware.assertLastAction(EngineAction.GoBackAction::class) {
                assertEquals(currentTab.id, it.tabId)
            }
            assertEquals("back", Events.browserToolbarAction.testGetValue()?.last()?.extra?.get("item"))

            toolbarStore.dispatch(backButton.onLongClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            navController.navigate(BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(null))
            assertEquals("back_long_press", Events.browserToolbarAction.testGetValue()?.last()?.extra?.get("item"))
        }
    }

    @Test
    fun `GIVEN the forward button is shown WHEN interacted with THEN go forward or show history and record telemetry`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        mockkStatic(Context::isLargeWindow) {
            every { any<Context>().isLargeWindow() } returns true
            every { settings.shouldUseBottomToolbar } returns false
            val currentTab = createTab("test.com", private = false)
            val captureMiddleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
            val browserStore = BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(currentTab),
                    selectedTabId = currentTab.id,
                ),
                middleware = listOf(captureMiddleware),
            )
            val middleware = buildMiddleware(appStore, browserStore = browserStore)
            val toolbarStore = buildStore(middleware)
            testScheduler.advanceUntilIdle()

             val forwardButton = toolbarStore.state.displayState.browserActionsStart[1] as ActionButtonRes
            toolbarStore.dispatch(forwardButton.onClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            captureMiddleware.assertLastAction(EngineAction.GoForwardAction::class) {
                assertEquals(currentTab.id, it.tabId)
            }
            assertEquals("forward", Events.browserToolbarAction.testGetValue()?.last()?.extra?.get("item"))

            toolbarStore.dispatch(forwardButton.onLongClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            navController.navigate(BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(null))
            assertEquals("forward_long_press", Events.browserToolbarAction.testGetValue()?.last()?.extra?.get("item"))
        }
    }

    @Test
    fun `GIVEN a top toolbar WHEN a website is loaded THEN show refresh button`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        mockkStatic(Context::isLargeWindow) {
            val browsingModeManager = SimpleBrowsingModeManager(Private)
            val currentNavDestination: NavDestination = mockk {
                every { id } returns R.id.browserFragment
            }
            val navController: NavController = mockk(relaxed = true) {
                every { currentDestination } returns currentNavDestination
            }

            every { any<Context>().isLargeWindow() } returns true
            every { settings.shouldUseBottomToolbar } returns true
            val currentTab = createTab("test.com", private = false)
            val browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(currentTab),
                    selectedTabId = currentTab.id,
                ),
            )
            val reloadUseCases: SessionUseCases.ReloadUrlUseCase = mockk(relaxed = true)
            val stopUseCases: SessionUseCases.StopLoadingUseCase = mockk(relaxed = true)
            val sessionUseCases: SessionUseCases = mockk {
                every { reload } returns reloadUseCases
                every { stopLoading } returns stopUseCases
            }
            val browserScreenStore = buildBrowserScreenStore()
            val browserUseCases: FenixBrowserUseCases = mockk(relaxed = true)
            val useCases: UseCases = mockk {
                every { fenixBrowserUseCases } returns browserUseCases
            }
            val middleware = buildMiddleware(
                appStore, browserScreenStore, browserStore, useCases, sessionUseCases = sessionUseCases,
            )
            val toolbarStore = buildStore(
                middleware, browsingModeManager = browsingModeManager, navController = navController,
            ).also {
                it.dispatch(BrowserToolbarAction.Init())
            }
            testScheduler.advanceUntilIdle()
            val loadUrlFlagsUsed = mutableListOf<LoadUrlFlags>()

            val pageLoadButton = toolbarStore.state.displayState.browserActionsStart.last() as ActionButtonRes
            assertEquals(expectedRefreshButton, pageLoadButton)
            toolbarStore.dispatch(pageLoadButton.onClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            verify { reloadUseCases(currentTab.id, capture(loadUrlFlagsUsed)) }
            assertEquals(LoadUrlFlags.none().value, loadUrlFlagsUsed.first().value)
            toolbarStore.dispatch(pageLoadButton.onLongClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            verify { reloadUseCases(currentTab.id, capture(loadUrlFlagsUsed)) }
            assertEquals(LoadUrlFlags.BYPASS_CACHE, loadUrlFlagsUsed.last().value)
            assertNotNull(AddressToolbar.reloadTapped.testGetValue())
        }
    }

    @Test
    fun `GIVEN a loaded tab WHEN the refresh button is pressed THEN show stop refresh button`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        mockkStatic(Context::isLargeWindow) {
            val browsingModeManager = SimpleBrowsingModeManager(Private)
            val currentNavDestination: NavDestination = mockk {
                every { id } returns R.id.browserFragment
            }
            val navController: NavController = mockk(relaxed = true) {
                every { currentDestination } returns currentNavDestination
            }
            every { any<Context>().isLargeWindow() } returns true
            every { settings.shouldUseBottomToolbar } returns false
            val currentTab = createTab("test.com", private = false)
            val browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(currentTab),
                    selectedTabId = currentTab.id,
                ),
            )
            val reloadUseCases: SessionUseCases.ReloadUrlUseCase = mockk(relaxed = true)
            val stopUseCases: SessionUseCases.StopLoadingUseCase = mockk(relaxed = true)
            val sessionUseCases: SessionUseCases = mockk {
                every { reload } returns reloadUseCases
                every { stopLoading } returns stopUseCases
            }
            val browserScreenStore = buildBrowserScreenStore()
            val browserUseCases: FenixBrowserUseCases = mockk(relaxed = true)
            val useCases: UseCases = mockk {
                every { fenixBrowserUseCases } returns browserUseCases
            }
            val middleware = buildMiddleware(
                appStore, browserScreenStore, browserStore, useCases, sessionUseCases = sessionUseCases,
            )
            val toolbarStore = buildStore(
                middleware, browsingModeManager = browsingModeManager, navController = navController,
            ).also {
                it.dispatch(BrowserToolbarAction.Init())
            }
            testScheduler.advanceUntilIdle()
            val loadUrlFlagsUsed = mutableListOf<LoadUrlFlags>()

            var pageLoadButton = toolbarStore.state.displayState.browserActionsStart.last() as ActionButtonRes
            assertEquals(expectedRefreshButton, pageLoadButton)
            toolbarStore.dispatch(pageLoadButton.onClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            verify { reloadUseCases(currentTab.id, capture(loadUrlFlagsUsed)) }
            assertEquals(LoadUrlFlags.none().value, loadUrlFlagsUsed.first().value)
            toolbarStore.dispatch(pageLoadButton.onLongClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            verify { reloadUseCases(currentTab.id, capture(loadUrlFlagsUsed)) }
            assertEquals(LoadUrlFlags.BYPASS_CACHE, loadUrlFlagsUsed.last().value)

            browserStore.dispatch(UpdateLoadingStateAction(currentTab.id, true)).joinBlocking()
            testScheduler.advanceUntilIdle()
            pageLoadButton = toolbarStore.state.displayState.browserActionsStart.last() as ActionButtonRes
            assertEquals(expectedStopButton, pageLoadButton)
            toolbarStore.dispatch(pageLoadButton.onClick as BrowserToolbarEvent)
            testScheduler.advanceUntilIdle()
            verify { stopUseCases(currentTab.id) }

            browserStore.dispatch(UpdateLoadingStateAction(currentTab.id, false)).joinBlocking()
            testScheduler.advanceUntilIdle()
            pageLoadButton = toolbarStore.state.displayState.browserActionsStart.last() as ActionButtonRes
            assertEquals(expectedRefreshButton, pageLoadButton)
        }
    }

    @Test
    fun `GIVEN the url if of a local file WHEN initializing the toolbar THEN add an appropriate security indicator`() = runTestOnMain {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            ),
        )
        val middleware = buildMiddleware(
            browserStore = browserStore,
            useCases = useCases,
        )
        every { tab.content.url } returns "content://test"
        val expectedSecurityIndicator = ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_page_portrait_24,
            contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
            onClick = StartPageActions.SiteInfoClicked,
        )

        val toolbarStore = buildStore(middleware)

        val toolbarPageActions = toolbarStore.state.displayState.pageActionsStart
        assertEquals(1, toolbarPageActions.size)
        val securityIndicator = toolbarPageActions[0] as ActionButtonRes
        assertEquals(expectedSecurityIndicator, securityIndicator)
    }

    @Test
    fun `GIVEN the website is secure WHEN initializing the toolbar THEN add an appropriate security indicator`() = runTestOnMain {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            ),
        )
        val middleware = buildMiddleware(
            browserStore = browserStore,
            useCases = useCases,
        )
        every { tab.content.securityInfo.secure } returns true
        val expectedSecurityIndicator = ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_shield_checkmark_24,
            contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
            onClick = StartPageActions.SiteInfoClicked,
        )

        val toolbarStore = buildStore(middleware)

        val toolbarPageActions = toolbarStore.state.displayState.pageActionsStart
        assertEquals(1, toolbarPageActions.size)
        val securityIndicator = toolbarPageActions[0] as ActionButtonRes
        assertEquals(expectedSecurityIndicator, securityIndicator)
    }

    @Test
    fun `GIVEN the website is insecure WHEN initializing the toolbar THEN add an appropriate security indicator`() = runTestOnMain {
        val middleware = buildMiddleware(
            browserStore = browserStore,
            useCases = useCases,
        )
        every { tab.content.securityInfo.secure } returns false
        val expectedSecurityIndicator = ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_shield_slash_24,
            contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
            onClick = StartPageActions.SiteInfoClicked,
        )

        val toolbarStore = buildStore(
            middleware = middleware,
        )

        val toolbarPageActions = toolbarStore.state.displayState.pageActionsStart
        assertEquals(1, toolbarPageActions.size)
        val securityIndicator = toolbarPageActions[0] as ActionButtonRes
        assertEquals(expectedSecurityIndicator, securityIndicator)
    }

    @Test
    fun `GIVEN the website is insecure WHEN the connection becomes secure THEN update appropriate security indicator`() =
        runTestOnMain {
            Dispatchers.setMain(StandardTestDispatcher())
            val tab = createTab(url = "URL", id = tabId)
            val browserStore = BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tab.id,
                ),
            )
            val middleware = buildMiddleware(
                browserStore = browserStore,
                useCases = useCases,
            )
            val expectedSecureIndicator = ActionButtonRes(
                drawableResId = R.drawable.mozac_ic_shield_checkmark_24,
                contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                onClick = StartPageActions.SiteInfoClicked,
            )
            val expectedInsecureIndicator = ActionButtonRes(
                drawableResId = R.drawable.mozac_ic_shield_slash_24,
                contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                onClick = StartPageActions.SiteInfoClicked,
            )
            val toolbarStore = buildStore(middleware).also {
                it.dispatch(BrowserToolbarAction.Init())
            }
            testScheduler.advanceUntilIdle()
            var toolbarPageActions = toolbarStore.state.displayState.pageActionsStart
            assertEquals(1, toolbarPageActions.size)
            var securityIndicator = toolbarPageActions[0] as ActionButtonRes
            assertEquals(expectedInsecureIndicator, securityIndicator)

            browserStore.dispatch(UpdateSecurityInfoAction(tab.id, SecurityInfoState(true)))
                .joinBlocking()
            testScheduler.advanceUntilIdle()
            toolbarPageActions = toolbarStore.state.displayState.pageActionsStart
            assertEquals(1, toolbarPageActions.size)
            securityIndicator = toolbarPageActions[0] as ActionButtonRes
            assertEquals(expectedSecureIndicator, securityIndicator)
        }

    @Test
    fun `GIVEN default state WHEN building NewTab action THEN returns NewTab ActionButton with DEFAULT state and no long-click`() {
        val middleware = buildMiddleware()
        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.NewTab,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_plus_24, result.drawableResId)
        assertEquals(R.string.home_screen_shortcut_open_new_tab_2, result.contentDescription)
        assertEquals(ActionButton.State.DEFAULT, result.state)
        assertEquals(AddNewTab, result.onClick)
        assertNull(result.onLongClick)
    }

    @Test
    fun `GIVEN no history WHEN building Back action THEN returns DISABLED Back ActionButton with long-click`() {
        val middleware = buildMiddleware()
        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Back,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_back_24, result.drawableResId)
        assertEquals(R.string.browser_menu_back, result.contentDescription)
        assertEquals(ActionButton.State.DISABLED, result.state)
        assertEquals(NavigateBackClicked, result.onClick)
        assertEquals(NavigateBackLongClicked, result.onLongClick)
    }

    @Test
    fun `GIVEN can go back WHEN building Back action THEN returns DEFAULT Back ActionButton`() {
        val contentState: ContentState = mockk(relaxed = true) {
            every { canGoBack } returns true
        }

        val tabSessionState: TabSessionState = mockk(relaxed = true) {
           every { content } returns contentState
        }

        val browserState = BrowserState(
            tabs = listOf(tabSessionState),
            selectedTabId = tabSessionState.id,
        )

        val browserStore = BrowserStore(browserState)
        val middleware = buildMiddleware(
            browserStore = browserStore,
        )

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Back,
        ) as ActionButtonRes

        assertEquals(ActionButton.State.DEFAULT, result.state)
    }

    @Test
    fun `GIVEN no history WHEN building Forward action THEN returns DISABLED Forward ActionButton with long-click`() {
        val middleware = buildMiddleware()
        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Forward,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_forward_24, result.drawableResId)
        assertEquals(R.string.browser_menu_forward, result.contentDescription)
        assertEquals(ActionButton.State.DISABLED, result.state)
        assertEquals(NavigateForwardClicked, result.onClick)
        assertEquals(NavigateForwardLongClicked, result.onLongClick)
    }

    @Test
    fun `GIVEN can go forward WHEN building Forward action THEN returns DEFAULT Forward ActionButton`() {
        val contentState: ContentState = mockk(relaxed = true) {
            every { canGoForward } returns true
        }

        val tabSessionState: TabSessionState = mockk(relaxed = true) {
            every { content } returns contentState
        }

        val browserState = BrowserState(
            tabs = listOf(tabSessionState),
            selectedTabId = tabSessionState.id,
        )

        val browserStore = BrowserStore(browserState)
        val middleware = buildMiddleware(browserStore = browserStore)

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Forward,
        ) as ActionButtonRes

        assertEquals(ActionButton.State.DEFAULT, result.state)
    }

    @Test
    fun `GIVEN not loading WHEN building RefreshOrStop action THEN returns Refresh ActionButton with both clicks`() {
        val contentState: ContentState = mockk(relaxed = true) {
            every { loading } returns false
        }

        val tabSessionState: TabSessionState = mockk(relaxed = true) {
            every { content } returns contentState
        }

        val browserState = BrowserState(
            tabs = listOf(tabSessionState),
            selectedTabId = tabSessionState.id,
        )

        val browserStore = BrowserStore(browserState)
        val middleware = buildMiddleware(browserStore = browserStore)

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.RefreshOrStop,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_arrow_clockwise_24, result.drawableResId)
        assertEquals(R.string.browser_menu_refresh, result.contentDescription)
        assertEquals(RefreshClicked(bypassCache = false), result.onClick)
        assertEquals(RefreshClicked(bypassCache = true), result.onLongClick)
    }

    @Test
    fun `GIVEN loading WHEN building RefreshOrStop action THEN returns Stop ActionButton`() {
        val contentState: ContentState = mockk(relaxed = true) {
            every { loading } returns true
        }

        val tabSessionState: TabSessionState = mockk(relaxed = true) {
            every { content } returns contentState
        }

        val browserState = BrowserState(
            tabs = listOf(tabSessionState),
            selectedTabId = tabSessionState.id,
        )

        val browserStore = BrowserStore(browserState)
        val middleware = buildMiddleware(browserStore = browserStore)

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.RefreshOrStop,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_cross_24, result.drawableResId)
        assertEquals(R.string.browser_menu_stop, result.contentDescription)
        assertEquals(StopRefreshClicked, result.onClick)
        assertNull(result.onLongClick)
    }

    @Test
    fun `GIVEN default state WHEN building Menu action THEN returns Menu ActionButton without long-click`() {
        val middleware = buildMiddleware()
        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Menu,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_ellipsis_vertical_24, result.drawableResId)
        assertEquals(R.string.content_description_menu, result.contentDescription)
        assertEquals(ActionButton.State.DEFAULT, result.state)
        assertEquals(MenuClicked, result.onClick)
        assertNull(result.onLongClick)
    }

    @Test
    fun `GIVEN reader mode inactive WHEN building ReaderMode action THEN returns DEFAULT ReaderMode ActionButton`() {
        val readerModeStatus: ReaderModeStatus = mockk(relaxed = true) {
            every { isAvailable } returns false
            every { isActive } returns false
        }

        every { browserScreenState.readerModeStatus } returns readerModeStatus
        val middleware = buildMiddleware()

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.ReaderMode,
        ) as ActionButtonRes

        assertEquals(R.drawable.ic_readermode, result.drawableResId)
        assertEquals(R.string.browser_menu_read, result.contentDescription)
        assertEquals(ActionButton.State.DEFAULT, result.state)
        assertEquals(ReaderModeClicked(false), result.onClick)
    }

    @Test
    fun `GIVEN reader mode active WHEN building ReaderMode action THEN returns ACTIVE ReaderMode ActionButton`() {
        val readerModeStatus: ReaderModeStatus = mockk(relaxed = true) {
            every { isAvailable } returns true
            every { isActive } returns true
        }

        every { browserScreenState.readerModeStatus } returns readerModeStatus
        val middleware = buildMiddleware()

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.ReaderMode,
        ) as ActionButtonRes

        assertEquals(R.string.browser_menu_read_close, result.contentDescription)
        assertEquals(ActionButton.State.ACTIVE, result.state)
        assertEquals(ReaderModeClicked(true), result.onClick)
    }

    @Test
    fun `GIVEN translation not done WHEN building Translate action THEN returns DEFAULT Translate ActionButton`() {
        val pageTranslationStatus: PageTranslationStatus = mockk(relaxed = true) {
            every { isTranslationPossible } returns true
            every { isTranslated } returns false
            every { isTranslateProcessing } returns false
        }

        every { browserScreenState.pageTranslationStatus } returns pageTranslationStatus
        val middleware = buildMiddleware()

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Translate,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_translate_24, result.drawableResId)
        assertEquals(R.string.browser_toolbar_translate, result.contentDescription)
        assertEquals(ActionButton.State.DEFAULT, result.state)
        assertEquals(TranslateClicked, result.onClick)
    }

    @Test
    fun `GIVEN already translated WHEN building Translate action THEN returns ACTIVE Translate ActionButton`() {
        val pageTranslationStatus: PageTranslationStatus = mockk(relaxed = true) {
            every { isTranslationPossible } returns true
            every { isTranslated } returns true
            every { isTranslateProcessing } returns false
        }

        every { browserScreenState.pageTranslationStatus } returns pageTranslationStatus
        val middleware = buildMiddleware()

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Translate,
        ) as ActionButtonRes

        assertEquals(ActionButton.State.ACTIVE, result.state)
    }

    @Test
    fun `GIVEN tabsCount set WHEN building TabCounter action THEN returns TabCounterAction with correct count`() {
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                    createTab(id = "c", url = "https://getpocket.com"),
                ),
            ),
        )

        val middleware = buildMiddleware(browserStore = browserStore)
        val store = buildStore(middleware)

        val action = middleware.buildAction(
            toolbarAction = ToolbarAction.TabCounter,
        ) as TabCounterAction

        assertEquals(3, action.count)
        assertEquals(
            testContext.getString(R.string.mozac_tab_counter_open_tab_tray, 3),
            action.contentDescription,
        )
        assertEquals(
            middleware.environment?.browsingModeManager?.mode == Private,
            action.showPrivacyMask,
        )
        assertEquals(TabCounterClicked, action.onClick)
        assertNotNull(action.onLongClick)
    }

    @Test
    fun `GIVEN in expanded mode WHEN THEN no browser end actions`() = runTestOnMain {
        every { settings.shouldUseSimpleToolbar } returns false
        Dispatchers.setMain(StandardTestDispatcher())
        val middleware = buildMiddleware(browserStore = browserStore)
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(0, toolbarBrowserActions.size)
    }

    @Test
    fun `WHEN building EditBookmark action THEN returns Bookmark ActionButton with correct icon`() {
        val middleware = buildMiddleware()
        buildStore(middleware)

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.EditBookmark,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_bookmark_fill_24, result.drawableResId)
        assertEquals(R.string.browser_menu_edit_bookmark, result.contentDescription)
        assertEquals(EditBookmarkClicked, result.onClick)
    }

    @Test
    fun `WHEN building Bookmark action THEN returns Bookmark ActionButton with correct icon`() {
        val middleware = buildMiddleware()
        buildStore(middleware)

        val result = middleware.buildAction(
            toolbarAction = ToolbarAction.Bookmark,
        ) as ActionButtonRes

        assertEquals(R.drawable.mozac_ic_bookmark_24, result.drawableResId)
        assertEquals(R.string.browser_menu_bookmark_this_page_2, result.contentDescription)
        assertEquals(AddBookmarkClicked, result.onClick)
    }

    private fun assertEqualsTabCounterButton(expected: TabCounterAction, actual: TabCounterAction) {
        assertEquals(expected.count, actual.count)
        assertEquals(expected.contentDescription, actual.contentDescription)
        assertEquals(expected.showPrivacyMask, actual.showPrivacyMask)
        assertEquals(expected.onClick, actual.onClick)
        when (expected.onLongClick) {
            null -> assertNull(actual.onLongClick)
            is BrowserToolbarEvent -> assertEquals(expected.onLongClick, actual.onLongClick)
            is BrowserToolbarMenu -> assertEquals(
                (expected.onLongClick as BrowserToolbarMenu).items(),
                (actual.onLongClick as BrowserToolbarMenu).items(),
            )
            is CombinedEventAndMenu -> {
                assertEquals(
                    (expected.onLongClick as CombinedEventAndMenu).event,
                    (actual.onLongClick as CombinedEventAndMenu).event,
                )
                assertEquals(
                    (expected.onLongClick as CombinedEventAndMenu).menu.items(),
                    (actual.onLongClick as CombinedEventAndMenu).menu.items(),
                )
            }
        }
    }

    private fun assertEqualsOrigin(expected: PageOrigin, actual: PageOrigin) {
        assertEquals(expected.hint, actual.hint)
        assertEquals(expected.url, actual.url.toString())
        assertEquals(expected.title, actual.title)
        assertEquals(expected.contextualMenuOptions, actual.contextualMenuOptions)
        assertEquals(expected.onClick, actual.onClick)
        assertEquals(expected.textGravity, actual.textGravity)
        assertEquals(expected.onLongClick, actual.onLongClick)
    }

    private val expectedRefreshButton = ActionButtonRes(
        drawableResId = R.drawable.mozac_ic_arrow_clockwise_24,
        contentDescription = R.string.browser_menu_refresh,
        state = ActionButton.State.DEFAULT,
        onClick = RefreshClicked(bypassCache = false),
        onLongClick = RefreshClicked(bypassCache = true),
    )

    private val expectedStopButton = ActionButtonRes(
        drawableResId = R.drawable.mozac_ic_cross_24,
        contentDescription = R.string.browser_menu_stop,
        state = ActionButton.State.DEFAULT,
        onClick = StopRefreshClicked,
    )

    private fun expectedReaderModeButton(isActive: Boolean = false) = ActionButtonRes(
        drawableResId = R.drawable.ic_readermode,
        contentDescription = when (isActive) {
            true -> R.string.browser_menu_read_close
            false -> R.string.browser_menu_read
        },
        state = when (isActive) {
            true -> ActionButton.State.ACTIVE
            false -> ActionButton.State.DEFAULT
        },
        onClick = ReaderModeClicked(isActive),
    )

    private val expectedGoForwardButton = ActionButtonRes(
        drawableResId = R.drawable.mozac_ic_forward_24,
        contentDescription = R.string.browser_menu_forward,
        state = ActionButton.State.ACTIVE,
        onClick = NavigateForwardClicked,
        onLongClick = NavigateForwardLongClicked,
    )

    private val expectedGoBackButton = ActionButtonRes(
        drawableResId = R.drawable.mozac_ic_back_24,
        contentDescription = R.string.browser_menu_back,
        state = ActionButton.State.ACTIVE,
        onClick = NavigateBackClicked,
        onLongClick = NavigateBackLongClicked,
    )

    private val expectedTranslateButton = ActionButtonRes(
        drawableResId = R.drawable.mozac_ic_translate_24,
        contentDescription = R.string.browser_toolbar_translate,
        onClick = TranslateClicked,
    )

    private fun expectedTabCounterButton(
        tabCount: Int = 0,
        isPrivate: Boolean = false,
        shouldUseBottomToolbar: Boolean = false,
    ) = TabCounterAction(
        count = tabCount,
        contentDescription = if (isPrivate) {
            testContext.getString(
                R.string.mozac_tab_counter_private,
                tabCount.toString(),
            )
        } else {
            testContext.getString(
                R.string.mozac_tab_counter_open_tab_tray,
                tabCount.toString(),
            )
        },
        showPrivacyMask = isPrivate,
        onClick = TabCounterClicked,
        onLongClick = CombinedEventAndMenu(TabCounterLongClicked) {
            listOf(
                BrowserToolbarMenuButton(
                    icon = DrawableResIcon(iconsR.drawable.mozac_ic_plus_24),
                    text = StringResText(R.string.mozac_browser_menu_new_tab),
                    contentDescription = StringResContentDescription(R.string.mozac_browser_menu_new_tab),
                    onClick = AddNewTab,
                ),

                BrowserToolbarMenuButton(
                    icon = DrawableResIcon(iconsR.drawable.mozac_ic_private_mode_24),
                    text = StringResText(R.string.mozac_browser_menu_new_private_tab),
                    contentDescription = StringResContentDescription(R.string.mozac_browser_menu_new_private_tab),
                    onClick = AddNewPrivateTab,
                ),

                BrowserToolbarMenuDivider,

                BrowserToolbarMenuButton(
                    icon = DrawableResIcon(iconsR.drawable.mozac_ic_cross_24),
                    text = StringResText(R.string.mozac_close_tab),
                    contentDescription = StringResContentDescription(R.string.mozac_close_tab),
                    onClick = CloseCurrentTab,
                ),
            ).apply {
                if (shouldUseBottomToolbar) {
                    asReversed()
                }
            }
        },
    )

    private val expectedNewTabButton = ActionButtonRes(
        drawableResId = R.drawable.mozac_ic_plus_24,
        contentDescription = R.string.home_screen_shortcut_open_new_tab_2,
        onClick = AddNewTab,
    )

    private val expectedMenuButton = ActionButtonRes(
        drawableResId = R.drawable.mozac_ic_ellipsis_vertical_24,
        contentDescription = R.string.content_description_menu,
        onClick = MenuClicked,
    )

    private fun buildMiddleware(
        appStore: AppStore = this.appStore,
        browserScreenStore: BrowserScreenStore = this.browserScreenStore,
        browserStore: BrowserStore = this.browserStore,
        useCases: UseCases = this.useCases,
        nimbusComponents: NimbusComponents = this.nimbusComponents,
        clipboard: ClipboardHandler = this.clipboard,
        publicSuffixList: PublicSuffixList = this.publicSuffixList,
        settings: Settings = this.settings,
        permissionsStorage: SitePermissionsStorage = this.permissionsStorage,
        cookieBannersStorage: CookieBannersStorage = this.cookieBannersStorage,
        trackingProtectionUseCases: TrackingProtectionUseCases = this.trackingProtectionUseCases,
        sessionUseCases: SessionUseCases = SessionUseCases(browserStore),
        bookmarksStorage: BookmarksStorage = this.bookmarksStorage,
    ) = BrowserToolbarMiddleware(
        appStore = appStore,
        browserScreenStore = browserScreenStore,
        browserStore = browserStore,
        useCases = useCases,
        nimbusComponents = nimbusComponents,
        clipboard = clipboard,
        publicSuffixList = publicSuffixList,
        settings = settings,
        permissionsStorage = permissionsStorage,
        cookieBannersStorage = cookieBannersStorage,
        trackingProtectionUseCases = trackingProtectionUseCases,
        sessionUseCases = sessionUseCases,
        tabManagementFeatureHelper = object : TabManagementFeatureHelper {
            override val enhancementsEnabledNightly: Boolean
                get() = false
            override val enhancementsEnabledBeta: Boolean
                get() = false
            override val enhancementsEnabledRelease: Boolean
                get() = false
            override val enhancementsEnabled: Boolean
                get() = false
        },
        bookmarksStorage = bookmarksStorage,
    )

    private fun buildStore(
        middleware: BrowserToolbarMiddleware = buildMiddleware(),
        context: Context = testContext,
        lifecycleOwner: LifecycleOwner = this@BrowserToolbarMiddlewareTest.lifecycleOwner,
        navController: NavController = this@BrowserToolbarMiddlewareTest.navController,
        browsingModeManager: BrowsingModeManager = this@BrowserToolbarMiddlewareTest.browsingModeManager,
        browserAnimator: BrowserAnimator = this@BrowserToolbarMiddlewareTest.browserAnimator,
        thumbnailsFeature: BrowserThumbnails? = this@BrowserToolbarMiddlewareTest.thumbnailsFeature,
        readerModeController: ReaderModeController = this@BrowserToolbarMiddlewareTest.readerModeController,
    ) = BrowserToolbarStore(
        middleware = listOf(middleware),
    ).also {
        it.dispatch(
            EnvironmentRehydrated(
                BrowserToolbarEnvironment(
                    context = context,
                    viewLifecycleOwner = lifecycleOwner,
                    navController = navController,
                    browsingModeManager = browsingModeManager,
                    browserAnimator = browserAnimator,
                    thumbnailsFeature = thumbnailsFeature,
                    readerModeController = readerModeController,
                ),
            ),
        )
    }

    private fun buildBrowserScreenStore(
        initialState: BrowserScreenState = BrowserScreenState(),
        middlewares: List<Middleware<BrowserScreenState, BrowserScreenAction>> = emptyList(),
        context: Context = testContext,
        viewLifecycleOwner: LifecycleOwner = lifecycleOwner,
        fragmentManager: FragmentManager = mockk(),
    ) = BrowserScreenStore(
        initialState = initialState,
        middleware = middlewares,
    ).also {
        it.dispatch(
            BrowserScreenAction.EnvironmentRehydrated(Environment(context, viewLifecycleOwner, fragmentManager)),
        )
    }

    private class FakeLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        override val lifecycle: Lifecycle = LifecycleRegistry(this).apply {
            currentState = initialState
        }
    }
}
