/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.appcompat.app.AlertDialog
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.spyk
import io.mockk.unmockkObject
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.components.search.BOOKMARKS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.HISTORY_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.TABS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.mozilla.fenix.search.SearchDialogFragmentDirections.Companion.actionCrashListFragment
import org.mozilla.fenix.search.SearchDialogFragmentDirections.Companion.actionGleanDebugToolsFragment
import org.mozilla.fenix.search.SearchDialogFragmentDirections.Companion.actionGlobalAddonsManagementFragment
import org.mozilla.fenix.search.SearchDialogFragmentDirections.Companion.actionGlobalBrowser
import org.mozilla.fenix.search.SearchDialogFragmentDirections.Companion.actionGlobalSearchEngineFragment
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class) // for gleanTestRule
class SearchDialogControllerTest {

    @MockK(relaxed = true)
    private lateinit var activity: HomeActivity

    @MockK(relaxed = true)
    private lateinit var store: SearchDialogFragmentStore

    @MockK(relaxed = true)
    private lateinit var navController: NavController

    @MockK private lateinit var searchEngine: SearchEngine

    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @MockK(relaxed = true)
    private lateinit var fenixBrowserUseCases: FenixBrowserUseCases

    private lateinit var middleware: CaptureActionsMiddleware<BrowserState, BrowserAction>
    private lateinit var browserStore: BrowserStore

    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        mockkObject(MetricsUtils)
        middleware = CaptureActionsMiddleware()
        browserStore = BrowserStore(
            middleware = listOf(middleware),
        )
        every { store.state.tabId } returns "test-tab-id"
        every { store.state.searchEngineSource.searchEngine } returns searchEngine
        every { searchEngine.type } returns SearchEngine.Type.BUNDLED
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }
        every { activity.components.nimbus.events } returns mockk()
        every { MetricsUtils.recordSearchMetrics(searchEngine, any(), any(), any()) } just Runs
    }

    @After
    fun teardown() {
        unmockkObject(MetricsUtils)
    }

    @Test
    fun `GIVEN default search engine is selected WHEN url is committed THEN load the url`() {
        val url = "https://www.google.com/"
        assertNull(Events.enteredUrl.testGetValue())

        every { store.state.defaultEngine } returns searchEngine

        createController().handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        verifyOrder {
            navController.navigate(actionGlobalBrowser())

            fenixBrowserUseCases.loadUrlOrSearch(
                searchTermOrURL = url,
                newTab = false,
                forceSearch = false,
                private = activity.browsingModeManager.mode.isPrivate,
                searchEngine = searchEngine,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }

        assertNotNull(Events.enteredUrl.testGetValue())
        val snapshot = Events.enteredUrl.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("false", snapshot.single().extra?.getValue("autocomplete"))
    }

    @Test
    fun `GIVEN default search engine is selected and homepage as a new tab is enabled WHEN url is committed THEN load the url`() {
        val url = "https://www.google.com/"
        assertNull(Events.enteredUrl.testGetValue())

        every { store.state.defaultEngine } returns searchEngine
        every { settings.enableHomepageAsNewTab } returns true
        every { store.state.tabId } returns null

        createController().handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        verifyOrder {
            navController.navigate(actionGlobalBrowser())

            fenixBrowserUseCases.loadUrlOrSearch(
                searchTermOrURL = url,
                newTab = false,
                forceSearch = false,
                private = activity.browsingModeManager.mode.isPrivate,
                searchEngine = searchEngine,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }

        assertNotNull(Events.enteredUrl.testGetValue())
        val snapshot = Events.enteredUrl.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("false", snapshot.single().extra?.getValue("autocomplete"))
    }

    @Test
    fun `GIVEN a general search engine is selected WHEN url is committed THEN perform search`() {
        val url = "https://www.google.com/"
        assertNull(Events.enteredUrl.testGetValue())

        every { store.state.defaultEngine } returns mockk(relaxed = true)

        createController().handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        verifyOrder {
            navController.navigate(actionGlobalBrowser())

            fenixBrowserUseCases.loadUrlOrSearch(
                searchTermOrURL = url,
                newTab = false,
                forceSearch = true,
                private = activity.browsingModeManager.mode.isPrivate,
                searchEngine = searchEngine,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }

        assertNotNull(Events.enteredUrl.testGetValue())
        val snapshot = Events.enteredUrl.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("false", snapshot.single().extra?.getValue("autocomplete"))
    }

    @Test
    fun handleBlankUrlCommitted() {
        val url = ""

        var dismissDialogInvoked = false
        createController(
            dismissDialog = {
                dismissDialogInvoked = true
            },
        ).handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        assertTrue(dismissDialogInvoked)

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertTrue(action.abandoned)
        }
    }

    @Test
    fun handleSearchCommitted() {
        val searchTerm = "Firefox"

        createController().handleUrlCommitted(searchTerm)

        browserStore.waitUntilIdle()

        verifyOrder {
            navController.navigate(actionGlobalBrowser())

            fenixBrowserUseCases.loadUrlOrSearch(
                searchTermOrURL = searchTerm,
                newTab = false,
                forceSearch = true,
                private = activity.browsingModeManager.mode.isPrivate,
                searchEngine = searchEngine,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun `GIVEN homepage as a new tab is enabled WHEN search term is committed THEN perform search in the existing tab`() {
        val searchTerm = "Firefox"

        every { settings.enableHomepageAsNewTab } returns true
        every { store.state.tabId } returns null

        createController().handleUrlCommitted(searchTerm)

        browserStore.waitUntilIdle()

        verifyOrder {
            navController.navigate(actionGlobalBrowser())

            fenixBrowserUseCases.loadUrlOrSearch(
                searchTermOrURL = searchTerm,
                newTab = false,
                forceSearch = true,
                private = activity.browsingModeManager.mode.isPrivate,
                searchEngine = searchEngine,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun `WHEN the search engine is added by the application THEN do not load URL`() {
        every { searchEngine.type } returns SearchEngine.Type.APPLICATION

        val searchTerm = "Firefox"
        var dismissDialogInvoked = false

        createController(
            dismissDialog = {
                dismissDialogInvoked = true
            },
        ).handleUrlCommitted(searchTerm)

        browserStore.waitUntilIdle()

        verify(exactly = 0) {
            activity.openToBrowserAndLoad(
                searchTermOrURL = any(),
                newTab = any(),
                from = any(),
                engine = any(),
            )
        }

        assertFalse(dismissDialogInvoked)

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)
    }

    @Test
    fun handleCrashesUrlCommitted() {
        val url = "about:crashes"
        val directions = actionCrashListFragment()

        createController().handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        verify { navController.navigate(directions) }
    }

    @Test
    fun handleAddonsUrlCommitted() {
        val url = "about:addons"
        val directions = actionGlobalAddonsManagementFragment()

        createController().handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        verify { navController.navigate(directions) }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun handleGleanUrlCommitted() {
        val url = "about:glean"
        val directions = actionGleanDebugToolsFragment()

        createController().handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        verify { navController.navigate(directions) }
    }

    @Test
    fun handleMozillaUrlCommitted() {
        val url = "moz://a"
        assertNull(Events.enteredUrl.testGetValue())

        every { store.state.defaultEngine } returns searchEngine

        createController().handleUrlCommitted(url)

        browserStore.waitUntilIdle()

        verifyOrder {
            navController.navigate(actionGlobalBrowser())

            fenixBrowserUseCases.loadUrlOrSearch(
                searchTermOrURL = SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.MANIFESTO),
                newTab = false,
                forceSearch = false,
                private = activity.browsingModeManager.mode.isPrivate,
                searchEngine = searchEngine,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }

        assertNotNull(Events.enteredUrl.testGetValue())
        val snapshot = Events.enteredUrl.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("false", snapshot.single().extra?.getValue("autocomplete"))
    }

    @Test
    fun handleEditingCancelled() = runTest {
        var clearToolbarFocusInvoked = false
        var dismissAndGoBack = false
        createController(
            clearToolbarFocus = {
                clearToolbarFocusInvoked = true
            },
            dismissDialogAndGoBack = {
                dismissAndGoBack = true
            },
        ).handleEditingCancelled()

        assertTrue(clearToolbarFocusInvoked)
        assertTrue(dismissAndGoBack)

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertTrue(action.abandoned)
        }
    }

    @Test
    fun handleTextChangedNonEmpty() {
        val text = "fenix"

        createController().handleTextChanged(text)

        browserStore.waitUntilIdle()

        verify { store.dispatch(SearchFragmentAction.UpdateQuery(text)) }

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)
    }

    @Test
    fun handleTextChangedEmpty() {
        val text = ""

        createController().handleTextChanged(text)

        browserStore.waitUntilIdle()

        verify { store.dispatch(SearchFragmentAction.UpdateQuery(text)) }

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)
    }

    @Test
    fun `WHEN felt privacy is enabled THEN do not dispatch AllowSearchSuggestionsInPrivateModePrompt`() {
        every { settings.feltPrivateBrowsingEnabled } returns true

        val text = "mozilla"

        createController().handleTextChanged(text)

        browserStore.waitUntilIdle()

        val actionSlot = mutableListOf<SearchFragmentAction>()
        verify { store.dispatch(capture(actionSlot)) }
        assertFalse(actionSlot.any { it is SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt })

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)
    }

    @Test
    fun `WHEN felt privacy is disabled THEN dispatch AllowSearchSuggestionsInPrivateModePrompt`() {
        every { settings.feltPrivateBrowsingEnabled } returns false

        val text = "mozilla"

        createController().handleTextChanged(text)

        browserStore.waitUntilIdle()

        val actionSlot = mutableListOf<SearchFragmentAction>()
        verify { store.dispatch(capture(actionSlot)) }
        assertTrue(actionSlot.any { it is SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt })

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)
    }

    @Test
    fun handleUrlTapped() {
        val url = "https://www.google.com/"
        val flags = EngineSession.LoadUrlFlags.all()
        assertNull(Events.enteredUrl.testGetValue())

        createController().handleUrlTapped(url, flags)
        createController().handleUrlTapped(url)

        browserStore.waitUntilIdle()

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                flags = flags,
            )
        }

        assertNotNull(Events.enteredUrl.testGetValue())
        val snapshot = Events.enteredUrl.testGetValue()!!
        assertEquals(2, snapshot.size)
        assertEquals("false", snapshot.first().extra?.getValue("autocomplete"))
        assertEquals("false", snapshot[1].extra?.getValue("autocomplete"))

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun `GIVEN homepage as a new tab is enabled WHEN an url suggestion is tapped THEN load url in the existing tab`() {
        val url = "https://www.google.com/"
        val flags = EngineSession.LoadUrlFlags.all()

        assertNull(Events.enteredUrl.testGetValue())

        every { settings.enableHomepageAsNewTab } returns true
        every { store.state.tabId } returns null

        createController().handleUrlTapped(url, flags)
        createController().handleUrlTapped(url)

        browserStore.waitUntilIdle()

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                flags = flags,
            )
        }

        assertNotNull(Events.enteredUrl.testGetValue())
        val snapshot = Events.enteredUrl.testGetValue()!!
        assertEquals(2, snapshot.size)
        assertEquals("false", snapshot.first().extra?.getValue("autocomplete"))
        assertEquals("false", snapshot[1].extra?.getValue("autocomplete"))

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun `GIVEN homepage as a new tab is enabled WHEN a search suggestion is tapped THEN perform search in the existing tab`() {
        val searchTerms = "fenix"

        every { settings.enableHomepageAsNewTab } returns true
        every { store.state.tabId } returns null

        createController().handleSearchTermsTapped(searchTerms)

        browserStore.waitUntilIdle()

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = searchTerms,
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                engine = searchEngine,
                forceSearch = true,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun handleSearchTermsTapped() {
        val searchTerms = "fenix"

        createController().handleSearchTermsTapped(searchTerms)

        browserStore.waitUntilIdle()

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = searchTerms,
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                engine = searchEngine,
                forceSearch = true,
            )
        }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun handleSearchShortcutEngineSelected() {
        val searchEngine: SearchEngine = mockk(relaxed = true)
        val browsingMode = BrowsingMode.Private
        every { activity.browsingModeManager.mode } returns browsingMode

        var focusToolbarInvoked = false
        createController(
            focusToolbar = {
                focusToolbarInvoked = true
            },
        ).handleSearchShortcutEngineSelected(searchEngine)

        browserStore.waitUntilIdle()

        assertTrue(focusToolbarInvoked)
        verify { store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine, browsingMode, settings)) }

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)

        assertNotNull(UnifiedSearch.engineSelected.testGetValue())
        val recordedEvents = UnifiedSearch.engineSelected.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        val eventExtra = recordedEvents.single().extra
        assertNotNull(eventExtra)
        assertTrue(eventExtra!!.containsKey("engine"))
        assertEquals(searchEngine.name, eventExtra["engine"])
    }

    @Test
    fun `WHEN history search engine is selected THEN dispatch correct action`() {
        val searchEngine: SearchEngine = mockk(relaxed = true)
        every { searchEngine.type } returns SearchEngine.Type.APPLICATION
        every { searchEngine.id } returns HISTORY_SEARCH_ENGINE_ID

        assertNull(UnifiedSearch.engineSelected.testGetValue())

        var focusToolbarInvoked = false
        createController(
            focusToolbar = {
                focusToolbarInvoked = true
            },
        ).handleSearchShortcutEngineSelected(searchEngine)

        browserStore.waitUntilIdle()

        assertTrue(focusToolbarInvoked)
        verify { store.dispatch(SearchFragmentAction.SearchHistoryEngineSelected(searchEngine)) }

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)

        assertNotNull(UnifiedSearch.engineSelected.testGetValue())
        val recordedEvents = UnifiedSearch.engineSelected.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        val eventExtra = recordedEvents.single().extra
        assertNotNull(eventExtra)
        assertTrue(eventExtra!!.containsKey("engine"))
        assertEquals("history", eventExtra["engine"])
    }

    @Test
    fun `WHEN bookmarks search engine is selected THEN dispatch correct action`() {
        val searchEngine: SearchEngine = mockk(relaxed = true)
        every { searchEngine.type } returns SearchEngine.Type.APPLICATION
        every { searchEngine.id } returns BOOKMARKS_SEARCH_ENGINE_ID

        assertNull(UnifiedSearch.engineSelected.testGetValue())

        var focusToolbarInvoked = false
        createController(
            focusToolbar = {
                focusToolbarInvoked = true
            },
        ).handleSearchShortcutEngineSelected(searchEngine)

        browserStore.waitUntilIdle()

        assertTrue(focusToolbarInvoked)
        verify { store.dispatch(SearchFragmentAction.SearchBookmarksEngineSelected(searchEngine)) }

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)

        assertNotNull(UnifiedSearch.engineSelected.testGetValue())
        val recordedEvents = UnifiedSearch.engineSelected.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        val eventExtra = recordedEvents.single().extra
        assertNotNull(eventExtra)
        assertTrue(eventExtra!!.containsKey("engine"))
        assertEquals("bookmarks", eventExtra["engine"])
    }

    @Test
    fun `WHEN tabs search engine is selected THEN dispatch correct action`() {
        val searchEngine: SearchEngine = mockk(relaxed = true)
        every { searchEngine.type } returns SearchEngine.Type.APPLICATION
        every { searchEngine.id } returns TABS_SEARCH_ENGINE_ID

        assertNull(UnifiedSearch.engineSelected.testGetValue())

        var focusToolbarInvoked = false
        createController(
            focusToolbar = {
                focusToolbarInvoked = true
            },
        ).handleSearchShortcutEngineSelected(searchEngine)

        browserStore.waitUntilIdle()

        assertTrue(focusToolbarInvoked)
        verify { store.dispatch(SearchFragmentAction.SearchTabsEngineSelected(searchEngine)) }

        middleware.assertNotDispatched(AwesomeBarAction.EngagementFinished::class)

        assertNotNull(UnifiedSearch.engineSelected.testGetValue())
        val recordedEvents = UnifiedSearch.engineSelected.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        val eventExtra = recordedEvents.single().extra
        assertNotNull(eventExtra)
        assertTrue(eventExtra!!.containsKey("engine"))
        assertEquals("tabs", eventExtra["engine"])
    }

    @Test
    fun handleClickSearchEngineSettings() {
        val directions: NavDirections = actionGlobalSearchEngineFragment()

        createController().handleClickSearchEngineSettings()

        browserStore.waitUntilIdle()

        verify { navController.navigate(directions) }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertTrue(action.abandoned)
        }
    }

    @Test
    fun handleExistingSessionSelected() {
        createController().handleExistingSessionSelected("selected")

        browserStore.waitUntilIdle()

        middleware.assertFirstAction(TabListAction.SelectTabAction::class) { action ->
            assertEquals("selected", action.tabId)
        }

        verify { activity.openToBrowser(from = BrowserDirection.FromSearchDialog) }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun handleExistingSessionSelected_tabId() {
        createController().handleExistingSessionSelected("tab-id")

        browserStore.waitUntilIdle()

        middleware.assertFirstAction(TabListAction.SelectTabAction::class) { action ->
            assertEquals("tab-id", action.tabId)
        }
        verify { activity.openToBrowser(from = BrowserDirection.FromSearchDialog) }

        middleware.assertLastAction(AwesomeBarAction.EngagementFinished::class) { action ->
            assertFalse(action.abandoned)
        }
    }

    @Test
    fun `show camera permissions needed dialog`() {
        val dialogBuilder: AlertDialog.Builder = mockk(relaxed = true)

        val spyController = spyk(createController())
        every { spyController.buildDialog() } returns dialogBuilder

        spyController.handleCameraPermissionsNeeded()

        verify { dialogBuilder.show() }
    }

    @Test
    fun `GIVEN search settings menu item WHEN search selector menu item is tapped THEN show search engine settings`() {
        val controller = spyk(createController())

        controller.handleMenuItemTapped(SearchSelectorMenu.Item.SearchSettings)

        verify { controller.handleClickSearchEngineSettings() }
    }

    private fun createController(
        clearToolbarFocus: () -> Unit = { },
        focusToolbar: () -> Unit = { },
        clearToolbar: () -> Unit = { },
        dismissDialog: () -> Unit = { },
        dismissDialogAndGoBack: () -> Unit = { },
    ): SearchDialogController {
        return SearchDialogController(
            activity = activity,
            store = browserStore,
            tabsUseCases = TabsUseCases(browserStore),
            fenixBrowserUseCases = fenixBrowserUseCases,
            fragmentStore = store,
            navController = navController,
            settings = settings,
            dismissDialog = dismissDialog,
            clearToolbarFocus = clearToolbarFocus,
            focusToolbar = focusToolbar,
            clearToolbar = clearToolbar,
            dismissDialogAndGoBack = dismissDialogAndGoBack,
        )
    }
}
