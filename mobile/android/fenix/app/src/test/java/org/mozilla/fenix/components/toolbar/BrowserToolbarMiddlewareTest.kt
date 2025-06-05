/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction.UpdateProgressAction
import mozilla.components.browser.state.action.TabListAction.AddTabAction
import mozilla.components.browser.state.action.TabListAction.RemoveTabAction
import mozilla.components.browser.state.ext.getUrl
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
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
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuDivider
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.ProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity.Bottom
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity.Top
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.utils.ClipboardHandler
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
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
import org.mozilla.fenix.browser.store.BrowserScreenAction.ClosingLastPrivateTab
import org.mozilla.fenix.browser.store.BrowserScreenAction.PageTranslationStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenAction.ReaderModeStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenState
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.UseCases
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.CurrentTabClosed
import org.mozilla.fenix.components.appstate.AppAction.SnackbarAction.SnackbarDismissed
import org.mozilla.fenix.components.appstate.AppAction.URLCopiedToClipboard
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.OrientationMode.Landscape
import org.mozilla.fenix.components.appstate.OrientationMode.Portrait
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.BrowserToolbarMiddleware.LifecycleDependencies
import org.mozilla.fenix.components.toolbar.DisplayActions.HomeClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.MenuClicked
import org.mozilla.fenix.components.toolbar.PageEndActionsInteractions.ReaderModeClicked
import org.mozilla.fenix.components.toolbar.PageEndActionsInteractions.TranslateClicked
import org.mozilla.fenix.components.toolbar.PageOriginInteractions.OriginClicked
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.AddNewPrivateTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.AddNewTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.CloseCurrentTab
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.utils.Settings
import org.robolectric.annotation.Config
import mozilla.components.ui.icons.R as iconsR

@RunWith(AndroidJUnit4::class)
class BrowserToolbarMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val appStore = AppStore()
    private val browserScreenStore: BrowserScreenStore = mockk(relaxed = true)
    private val browserStore = BrowserStore()
    private val clipboard: ClipboardHandler = mockk()
    private val lifecycleOwner = FakeLifecycleOwner(Lifecycle.State.RESUMED)
    private val navController: NavController = mockk(relaxed = true)
    private val browsingModeManager = SimpleBrowsingModeManager(Normal)
    private val browserAnimator: BrowserAnimator = mockk()
    private val thumbnailsFeature: BrowserThumbnails = mockk()
    private val readerModeController: ReaderModeController = mockk()
    private val useCases: UseCases = mockk()
    private val settings: Settings = mockk {
        every { shouldUseBottomToolbar } returns true
    }

    @Test
    fun `WHEN initializing the toolbar THEN add browser start actions`() = runTestOnMain {
        val middleware = buildMiddleware().updateDependencies()

        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsStart
        assertEquals(listOf(expectedHomeButton), toolbarBrowserActions)
    }

    @Test
    fun `WHEN initializing the toolbar THEN add browser end actions`() = runTestOnMain {
        val middleware = buildMiddleware().updateDependencies()

        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
        val tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
        val menuButton = toolbarBrowserActions[1] as ActionButton
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
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            browsingModeManager = browsingModeManager,
        )

        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        val tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
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
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            browsingModeManager = browsingModeManager,
        )

        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        val tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(2, true), tabCounterButton)
    }

    @Test
    fun `GIVEN WHEN initializing the toolbar THEN setup showing the website origin`() {
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
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies()

        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        val originConfiguration = toolbarStore.state.displayState.pageOrigin
        assertEquals(expectedConfiguration, originConfiguration)
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
        val middleware = buildMiddleware(appStore = appStore).updateDependencies()
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)

        appStore.dispatch(AppAction.OrientationChange(Landscape)).joinBlocking()
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
        val tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
        val menuButton = toolbarBrowserActions[1] as ActionButton
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
        val middleware = buildMiddleware(appStore = appStore).updateDependencies()

        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
        val tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
        val menuButton = toolbarBrowserActions[1] as ActionButton
        assertEqualsTabCounterButton(expectedTabCounterButton(), tabCounterButton)
        assertEquals(expectedMenuButton, menuButton)

        // In portrait the navigation bar is displayed
        appStore.dispatch(AppAction.OrientationChange(Portrait)).joinBlocking()
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
    }

    @Test
    fun `GIVEN in normal browsing WHEN the number of normal opened tabs is modified THEN update the tab counter`() = runTestOnMain {
        Dispatchers.setMain(StandardTestDispatcher())
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val browserStore = BrowserStore()
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
        var tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0), tabCounterButton)

        val newNormalTab = createTab("test.com", private = false)
        val newPrivateTab = createTab("test.com", private = true)
        browserStore.dispatch(AddTabAction(newNormalTab)).joinBlocking()
        browserStore.dispatch(AddTabAction(newPrivateTab)).joinBlocking()
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
        tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
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
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        testScheduler.advanceUntilIdle()
        var toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
        var tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)

        browserStore.dispatch(RemoveTabAction(initialPrivateTab.id)).joinBlocking()
        testScheduler.advanceUntilIdle()

        toolbarBrowserActions = toolbarStore.state.displayState.browserActionsEnd
        assertEquals(2, toolbarBrowserActions.size)
        tabCounterButton = toolbarBrowserActions[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0, true), tabCounterButton)
    }

    @Test
    fun `WHEN clicking the home button THEN navigate to application's home screen`() {
        val navController: NavController = mockk(relaxed = true)
        val browserAnimatorActionCaptor = slot<(Boolean) -> Unit>()
        every {
            browserAnimator.captureEngineViewAndDrawStatically(
                any<Long>(),
                capture(browserAnimatorActionCaptor),
            )
        } answers { browserAnimatorActionCaptor.captured.invoke(true) }
        val middleware = buildMiddleware().updateDependencies(
            navController = navController,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val homeButton = toolbarStore.state.displayState.browserActionsStart[0] as ActionButton

        mockkStatic(NavController::nav) {
            toolbarStore.dispatch(homeButton.onClick as BrowserToolbarEvent)

            verify { browserAnimator.captureEngineViewAndDrawStatically(any(), any()) }
            verify { navController.navigate(BrowserFragmentDirections.actionGlobalHome()) }
        }
    }

    @Test
    fun `WHEN clicking the menu button THEN open the menu`() {
        val navController: NavController = mockk(relaxed = true)
        val middleware = buildMiddleware().updateDependencies(
            navController = navController,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val menuButton = toolbarStore.state.displayState.browserActionsEnd[1] as ActionButton

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
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val thumbnailsFeature: BrowserThumbnails = mockk(relaxed = true)
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
            thumbnailsFeature = thumbnailsFeature,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction

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
    }

    @Test
    fun `GIVEN browsing in private mode WHEN clicking the tab counter button THEN open the tabs tray in private mode`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val thumbnailsFeature: BrowserThumbnails = mockk(relaxed = true)
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
            thumbnailsFeature = thumbnailsFeature,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction

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
    }

    @Test
    fun `WHEN clicking on the first option in the toolbar long click menu THEN open a new normal tab`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0, false), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as BrowserToolbarMenu).items()

        toolbarStore.dispatch((tabCounterMenuItems[0] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Normal, browsingModeManager.mode)
        verify {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
            )
        }
    }

    @Test
    fun ` WHEN the page origin is clicked THEN enter is edit mode`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val middleware = buildMiddleware(browserStore = browserStore).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        assertFalse(toolbarStore.state.isEditMode())

        toolbarStore.dispatch(toolbarStore.state.displayState.pageOrigin.onClick)

        assertTrue(toolbarStore.state.isEditMode())
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        toolbarStore.dispatch(CopyToClipboardClicked)

        assertEquals(currentTab.getUrl(), clipboard.text)
        verify { appStore.dispatch(URLCopiedToClipboard) }
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        toolbarStore.dispatch(CopyToClipboardClicked)

        assertEquals(currentTab.getUrl(), clipboard.text)
        verify { appStore.dispatch(URLCopiedToClipboard) }
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        toolbarStore.dispatch(CopyToClipboardClicked)

        assertEquals(currentTab.getUrl(), clipboard.text)
        verify(exactly = 0) { appStore.dispatch(URLCopiedToClipboard) }
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
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
    }

    @Test
    fun `WHEN clicking on the second option in the toolbar long click menu THEN open a new private tab`() {
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Normal)
        val middleware = buildMiddleware().updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(0, false), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as BrowserToolbarMenu).items()

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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(2, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as BrowserToolbarMenu).items()

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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, false), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as BrowserToolbarMenu).items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Normal, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(any())
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
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val appStore: AppStore = mockk(relaxed = true)
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as BrowserToolbarMenu).items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(any())
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
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val appStore: AppStore = mockk(relaxed = true)
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as BrowserToolbarMenu).items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(any())
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
        val navController: NavController = mockk(relaxed = true)
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val appStore: AppStore = mockk(relaxed = true)
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
        ).updateDependencies(
            navController = navController,
            browsingModeManager = browsingModeManager,
        )
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        val tabCounterButton = toolbarStore.state.displayState.browserActionsEnd[0] as TabCounterAction
        assertEqualsTabCounterButton(expectedTabCounterButton(1, true), tabCounterButton)
        val tabCounterMenuItems = (tabCounterButton.onLongClick as BrowserToolbarMenu).items()

        toolbarStore.dispatch((tabCounterMenuItems[3] as BrowserToolbarMenuButton).onClick!!)

        assertEquals(Private, browsingModeManager.mode)
        verify(exactly = 0) {
            tabsUseCases.removeTab(any(), any())
            appStore.dispatch(any())
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
        ).updateDependencies()
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        ).also {
            it.dispatch(BrowserToolbarAction.Init())
        }

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
        ).updateDependencies()
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        ).also {
            it.dispatch(BrowserToolbarAction.Init())
        }

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
        val browserScreenStore = BrowserScreenStore()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val middleware = buildMiddleware(
            browserScreenStore = browserScreenStore,
            browserStore = browserStore,
        ).updateDependencies(readerModeController = readerModeController)
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        browserScreenStore.dispatch(
            ReaderModeStatusUpdated(
                ReaderModeStatus(
                    isAvailable = true,
                    isActive = false,
                ),
            ),
        )

        val readerModeButton = toolbarStore.state.displayState.pageActionsEnd[0] as ActionButton
        assertEquals(expectedReaderModeButton(false), readerModeButton)

        toolbarStore.dispatch(readerModeButton.onClick as BrowserToolbarEvent)
        verify { readerModeController.showReaderView() }
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
        val browserScreenStore = BrowserScreenStore()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val middleware = buildMiddleware(
            browserScreenStore = browserScreenStore,
            browserStore = browserStore,
        ).updateDependencies(readerModeController = readerModeController)
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        browserScreenStore.dispatch(
            ReaderModeStatusUpdated(
                ReaderModeStatus(
                    isAvailable = true,
                    isActive = true,
                ),
            ),
        )

        val readerModeButton = toolbarStore.state.displayState.pageActionsEnd[0] as ActionButton
        assertEquals(expectedReaderModeButton(true), readerModeButton)

        toolbarStore.dispatch(readerModeButton.onClick as BrowserToolbarEvent)
        verify { readerModeController.hideReaderView() }
    }

    @Test
    fun `WHEN translation is possible THEN show a translate button`() {
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val navController: NavController = mockk(relaxed = true)
        val appStore: AppStore = mockk(relaxed = true)
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
            ),
        )
        val browserScreenStore = BrowserScreenStore()
        val middleware = BrowserToolbarMiddleware(
            appStore = appStore,
            browserScreenStore = browserScreenStore,
            browserStore = browserStore,
            useCases = useCases,
            clipboard = mockk(),
            settings = settings,
        ).apply {
            updateLifecycleDependencies(
                LifecycleDependencies(
                    context = testContext,
                    lifecycleOwner = lifecycleOwner,
                    navController = navController,
                    browsingModeManager = browsingModeManager,
                    browserAnimator = mockk(),
                    thumbnailsFeature = mockk(),
                    readerModeController = mockk(),
                ),
            )
        }
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

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
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val navController: NavController = mockk(relaxed = true)
        val appStore: AppStore = mockk(relaxed = true)
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
            ),
        )
        val browserScreenStore = BrowserScreenStore()
        val middleware = BrowserToolbarMiddleware(
            appStore = appStore,
            browserScreenStore = browserScreenStore,
            browserStore = browserStore,
            useCases = useCases,
            clipboard = mockk(),
            settings = settings,
        ).apply {
            updateLifecycleDependencies(
                LifecycleDependencies(
                    context = testContext,
                    lifecycleOwner = lifecycleOwner,
                    navController = navController,
                    browsingModeManager = browsingModeManager,
                    browserAnimator = mockk(),
                    thumbnailsFeature = mockk(),
                    readerModeController = mockk(),
                ),
            )
        }
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

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
        val browsingModeManager = SimpleBrowsingModeManager(Private)
        val currentNavDestination: NavDestination = mockk {
            every { id } returns R.id.browserFragment
        }
        val navController: NavController = mockk(relaxed = true) {
            every { currentDestination } returns currentNavDestination
        }
        val appStore: AppStore = mockk(relaxed = true)
        val currentTab = createTab("test.com", private = true)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(currentTab, createTab("firefox.com")),
                selectedTabId = currentTab.id,
            ),
        )
        val browserScreenStore = BrowserScreenStore()
        val middleware = BrowserToolbarMiddleware(
            appStore = appStore,
            browserScreenStore = browserScreenStore,
            browserStore = browserStore,
            useCases = useCases,
            clipboard = mockk(),
            settings = settings,
        ).apply {
            updateLifecycleDependencies(
                LifecycleDependencies(
                    context = testContext,
                    lifecycleOwner = lifecycleOwner,
                    navController = navController,
                    browsingModeManager = browsingModeManager,
                    browserAnimator = mockk(),
                    thumbnailsFeature = mockk(),
                    readerModeController = mockk(),
                ),
            )
        }
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )
        browserScreenStore.dispatch(
            PageTranslationStatusUpdated(
                PageTranslationStatus(
                    isTranslationPossible = true,
                    isTranslated = false,
                    isTranslateProcessing = false,
                ),
            ),
        )

        val translateButton = toolbarStore.state.displayState.pageActionsEnd[0] as ActionButton
        toolbarStore.dispatch(translateButton.onClick as BrowserToolbarEvent)

        verify { appStore.dispatch(SnackbarDismissed) }
        verify { navController.navigate(BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment()) }
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

    private fun expectedReaderModeButton(isActive: Boolean = false) = ActionButton(
        icon = R.drawable.ic_readermode,
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

    private val expectedTranslateButton = ActionButton(
        icon = R.drawable.mozac_ic_translate_24,
        contentDescription = R.string.browser_toolbar_translate,
        onClick = TranslateClicked,
    )

    private fun expectedTabCounterButton(
        tabCount: Int = 0,
        isPrivate: Boolean = false,
        shouldUseBottomToolbar: Boolean = false,
    ) = TabCounterAction(
        count = tabCount,
        contentDescription = testContext.getString(R.string.mozac_tab_counter_open_tab_tray, tabCount),
        showPrivacyMask = isPrivate,
        onClick = TabCounterInteractions.TabCounterClicked,
        onLongClick = BrowserToolbarMenu {
            listOf(
                BrowserToolbarMenuButton(
                    iconResource = iconsR.drawable.mozac_ic_plus_24,
                    text = R.string.mozac_browser_menu_new_tab,
                    contentDescription = R.string.mozac_browser_menu_new_tab,
                    onClick = AddNewTab,
                ),
                BrowserToolbarMenuButton(
                    iconResource = iconsR.drawable.mozac_ic_private_mode_24,
                    text = R.string.mozac_browser_menu_new_private_tab,
                    contentDescription = R.string.mozac_browser_menu_new_private_tab,
                    onClick = AddNewPrivateTab,
                ),

                BrowserToolbarMenuDivider,

                BrowserToolbarMenuButton(
                    iconResource = iconsR.drawable.mozac_ic_cross_24,
                    text = R.string.mozac_close_tab,
                    contentDescription = R.string.mozac_close_tab,
                    onClick = CloseCurrentTab,
                ),
            ).apply {
                if (shouldUseBottomToolbar) {
                    asReversed()
                }
            }
        },
    )

    private val expectedHomeButton = ActionButton(
        icon = R.drawable.mozac_ic_home_24,
        contentDescription = R.string.browser_toolbar_home,
        onClick = HomeClicked,
    )

    private val expectedMenuButton = ActionButton(
        icon = R.drawable.mozac_ic_ellipsis_vertical_24,
        contentDescription = R.string.content_description_menu,
        onClick = MenuClicked,
    )

    private fun buildMiddleware(
        appStore: AppStore = this.appStore,
        browserScreenStore: BrowserScreenStore = this.browserScreenStore,
        browserStore: BrowserStore = this.browserStore,
        useCases: UseCases = this.useCases,
        clipboard: ClipboardHandler = this.clipboard,
        settings: Settings = this.settings,
    ) = BrowserToolbarMiddleware(
        appStore = appStore,
        browserScreenStore = browserScreenStore,
        browserStore = browserStore,
        useCases = useCases,
        clipboard = clipboard,
        settings = settings,
    )

    private fun BrowserToolbarMiddleware.updateDependencies(
        context: Context = testContext,
        lifecycleOwner: LifecycleOwner = this@BrowserToolbarMiddlewareTest.lifecycleOwner,
        navController: NavController = this@BrowserToolbarMiddlewareTest.navController,
        browsingModeManager: BrowsingModeManager = this@BrowserToolbarMiddlewareTest.browsingModeManager,
        browserAnimator: BrowserAnimator = this@BrowserToolbarMiddlewareTest.browserAnimator,
        thumbnailsFeature: BrowserThumbnails? = this@BrowserToolbarMiddlewareTest.thumbnailsFeature,
        readerModeController: ReaderModeController = this@BrowserToolbarMiddlewareTest.readerModeController,
    ) = this.apply {
        updateLifecycleDependencies(
            LifecycleDependencies(
                context = context,
                lifecycleOwner = lifecycleOwner,
                navController = navController,
                browsingModeManager = browsingModeManager,
                browserAnimator = browserAnimator,
                thumbnailsFeature = thumbnailsFeature,
                readerModeController = readerModeController,
            ),
        )
    }

    private class FakeLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        override val lifecycle: Lifecycle = LifecycleRegistry(this).apply {
            currentState = initialState
        }
    }
}
