/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Build
import android.view.ViewGroup
import android.view.Window
import android.widget.FrameLayout
import android.widget.ImageButton
import android.widget.TextView
import androidx.core.content.ContextCompat.getColor
import androidx.core.view.forEach
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ColorSchemeParams
import mozilla.components.browser.state.state.ColorSchemes
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class CustomTabsToolbarFeatureTest {
    @Test
    fun `start without sessionId invokes nothing`() {
        val store = BrowserStore()
        val toolbar: BrowserToolbar = mock()
        val display: DisplayToolbar = mock()
        val colors: DisplayToolbar.Colors = mock()
        whenever(toolbar.display).thenReturn(display)
        whenever(display.colors).thenReturn(colors)
        whenever(colors.menu).thenReturn(0)

        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = null, useCases = useCases) {})

        feature.start()

        verify(feature, never()).init(any())
    }

    @Test
    fun `start calls initialize with the sessionId`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla")

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).init(tab.config)

        // Calling start again should NOT call init again

        feature.start()

        verify(feature, times(1)).init(tab.config)
    }

    @Test
    fun `initialize updates toolbar`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla")

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {}

        feature.init(tab.config)

        assertFalse(toolbar.display.onUrlClicked.invoke())
    }

    @Suppress("DEPRECATION")
    @Test
    @Config(sdk = [28])
    fun `initialize updates toolbar, window and text color on SDK 28`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(
                        toolbarColor = Color.RED,
                        navigationBarColor = Color.BLUE,
                    ),
                ),
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases, window = window) {}

        feature.init(tab.config)

        verify(toolbar).setBackgroundColor(Color.RED)
        verify(window).statusBarColor = Color.RED
        verify(window).navigationBarColor = Color.BLUE

        assertEquals(Color.WHITE, toolbar.display.colors.title)
        assertEquals(Color.WHITE, toolbar.display.colors.text)
    }

    @Config(sdk = [Build.VERSION_CODES.UPSIDE_DOWN_CAKE])
    @Suppress("DEPRECATION")
    @Test
    fun `initialize updates toolbar, window and text color on SDK lower than 35`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(
                        toolbarColor = Color.RED,
                        navigationBarColor = Color.BLUE,
                    ),
                ),
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases, window = window) {}

        feature.init(tab.config)

        verify(toolbar).setBackgroundColor(Color.RED)
        verify(window).statusBarColor = Color.RED
        verify(window).navigationBarColor = Color.BLUE

        assertEquals(Color.WHITE, toolbar.display.colors.title)
        assertEquals(Color.WHITE, toolbar.display.colors.text)
    }

    @Suppress("DEPRECATION")
    @Test
    fun `initialize updates toolbar, window and text color on SDK 35`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(
                        toolbarColor = Color.RED,
                        navigationBarColor = Color.BLUE,
                    ),
                ),
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases, window = window) {}

        feature.init(tab.config)

        verify(window, never()).navigationBarColor = Color.RED
        verify(window, never()).statusBarColor = Color.RED
        verify(window, never()).navigationBarColor = Color.BLUE

        assertEquals(Color.WHITE, toolbar.display.colors.title)
        assertEquals(Color.WHITE, toolbar.display.colors.text)

        assertEquals(window.statusBarColor, Color.TRANSPARENT)
        assertEquals(window.navigationBarColor, Color.TRANSPARENT)
    }

    @Test
    fun `initialize does not update toolbar colors if this functionality is disabled`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.RED),
                ),
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())
        val initialDisplayToolbarColors = toolbar.display.colors

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateToolbarsColor = false,
                ),
            ) {}

            feature.init(tab.config)

            verify(toolbar, never()).setBackgroundColor(Color.RED)
            assertSame(initialDisplayToolbarColors, toolbar.display.colors)
        }

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateToolbarsColor = true,
                ),
            ) {}

            feature.init(tab.config)

            verify(toolbar).setBackgroundColor(Color.RED)
            assertNotSame(initialDisplayToolbarColors, toolbar.display.colors)
        }
    }

    @Config(sdk = [Build.VERSION_CODES.UPSIDE_DOWN_CAKE])
    @Suppress("DEPRECATION")
    @Test
    fun `GIVEN changing the status bar color is enabled WHEN customizing the UI for a custom tab on SDK lower than 35 THEN change the status bar color`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.GREEN),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateStatusBarColor = true,
                ),
            ) {}

            feature.init(tab.config)

            verify(window).statusBarColor = Color.GREEN
        }
    }

    @Suppress("DEPRECATION")
    @Test
    fun `GIVEN changing the status bar color is enabled WHEN customizing the UI for a custom tab on SDK 35 THEN the status bar color is not changed`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.GREEN),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateStatusBarColor = true,
                ),
            ) {}

            feature.init(tab.config)

            verify(window, never()).navigationBarColor = Color.GREEN
            assertEquals(window.navigationBarColor, Color.TRANSPARENT)
        }
    }

    @Suppress("DEPRECATION")
    @Test
    fun `GIVEN changing the status bar color is disabled WHEN customizing the UI for a custom tab THEN don't change the status bar color`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.GREEN),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateStatusBarColor = false,
                ),
            ) {}

            feature.init(tab.config)

            verify(window, never()).statusBarColor = Color.GREEN
        }
    }

    @Config(sdk = [Build.VERSION_CODES.UPSIDE_DOWN_CAKE])
    @Suppress("DEPRECATION")
    @Test
    fun `GIVEN changing the system navigation bar color is enabled WHEN customizing the UI for a custom tab on SDK lower than 35 THEN change the system navigation bar color`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.BLUE),
                ),
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateSystemNavigationBarColor = true,
                ),
            ) {}

            feature.init(tab.config)

            verify(window).navigationBarColor = Color.BLUE
        }
    }

    @Test
    @Suppress("DEPRECATION")
    fun `GIVEN changing the system navigation bar color is enabled WHEN customizing the UI for a custom tab on SDK 35 THEN change the system navigation bar color is not called`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.BLUE),
                ),
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateSystemNavigationBarColor = true,
                ),
            ) {}

            feature.init(tab.config)

            verify(window, never()).navigationBarColor = eq(Color.BLUE)
            assertEquals(window.navigationBarColor, Color.TRANSPARENT)
        }
    }

    @Suppress("DEPRECATION")
    @Test
    fun `GIVEN changing the system navigation bar color is disabled WHEN customizing the UI for a custom tab THEN don't change the system navigation bar color`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.BLUE),
                ),
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        `when`(window.context).thenReturn(testContext)
        `when`(window.insetsController).thenReturn(mock())

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                customTabsColorsConfig = CustomTabsColorsConfig(
                    updateSystemNavigationBarColor = false,
                ),
            ) {}

            feature.init(tab.config)

            verify(window, never()).navigationBarColor = Color.BLUE
        }
    }

    @Test
    fun `adds close button`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {}

        feature.start()

        verify(toolbar).addNavigationAction(any())
    }

    @Test
    fun `doesn't add close button if the button should be hidden`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                showCloseButton = false,
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {}

        feature.start()

        verify(toolbar, never()).addNavigationAction(any())
    }

    @Test
    fun `close button invokes callback and removes session`() {
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()

        val store = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                customTabs = listOf(
                    createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig()),
                ),
            ),
        )

        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeClicked = false
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {
            closeClicked = true
        }

        feature.start()

        verify(toolbar).addNavigationAction(any())

        val button = extractActionView(toolbar, testContext.getString(R.string.mozac_feature_customtabs_exit_button))

        middleware.assertNotDispatched(CustomTabListAction.RemoveCustomTabAction::class)

        button?.performClick()

        assertTrue(closeClicked)

        middleware.assertLastAction(CustomTabListAction.RemoveCustomTabAction::class) { action ->
            assertEquals("mozilla", action.tabId)
        }
    }

    @Test
    fun `GIVEN default custom tabs setting THEN refresh button does not appear`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
            ) {},
        )

        feature.start()

        verify(feature, never()).addRefreshButton(anyInt())
        verify(toolbar, never()).addBrowserAction(any())
    }

    @Test
    fun `GIVEN custom tab setting with refresh listener and flag THEN refresh button does appear`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                customTabsToolbarListeners = CustomTabsToolbarListeners(
                    refreshListener = {},
                ),
                customTabsToolbarButtonConfig = CustomTabsToolbarButtonConfig(
                    showRefreshButton = true,
                ),
            ) {},
        )

        feature.start()

        verify(feature).addRefreshButton(anyInt())
    }

    @Test
    fun `GIVEN custom tabs setting with refresh button listener and flag THEN Refresh button uses custom refresh listener`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var clicked = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                customTabsToolbarListeners = CustomTabsToolbarListeners(
                    refreshListener = { clicked = true },
                ),
                customTabsToolbarButtonConfig = CustomTabsToolbarButtonConfig(
                    showRefreshButton = true,
                ),
            ) {},
        )

        feature.start()

        verify(feature).addRefreshButton(anyInt())

        val captor = argumentCaptor<Toolbar.ActionButton>()
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        button.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `GIVEN the default custom tabs toolbar button config and listeners THEN do not add menu button`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store = store,
                toolbar = toolbar,
                sessionId = "mozilla",
                useCases = useCases,
            ) {},
        )

        feature.start()

        verify(feature, never()).addMenuButton()
        verify(toolbar, never()).addBrowserAction(any())
    }

    @Test
    fun `GIVEN custom tabs toolbar config to show menu with a menu listener THEN show menu button with custom menu listener`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var clicked = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store = store,
                toolbar = toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                customTabsToolbarButtonConfig = CustomTabsToolbarButtonConfig(
                    showMenu = true,
                ),
                customTabsToolbarListeners = CustomTabsToolbarListeners(
                    menuListener = { clicked = true },
                ),
            ) {},
        )

        feature.start()

        verify(feature).addMenuButton()

        val captor = argumentCaptor<Toolbar.ActionButton>()
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        button.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `does not add share button by default`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature, never()).addShareButton(anyInt())
        verify(toolbar, never()).addBrowserAction(any())
    }

    @Test
    fun `adds share button`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                showShareMenuItem = true,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addShareButton(anyInt())
        verify(toolbar).addBrowserAction(any())
    }

    @Test
    fun `share button uses custom share listener`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                showShareMenuItem = true,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var clicked = false
        val feature = CustomTabsToolbarFeature(
            store,
            toolbar,
            sessionId = "mozilla",
            useCases = useCases,
            customTabsToolbarListeners = CustomTabsToolbarListeners(
                shareListener = { clicked = true },
            ),
        ) {}

        feature.start()

        val captor = argumentCaptor<Toolbar.ActionButton>()
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        button.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `initialize calls addActionButton`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addActionButton(anyInt(), any())
    }

    @Test
    fun `GIVEN a square icon larger than the max drawable size WHEN adding action button to toolbar THEN the icon is scaled to fit`() {
        val captor = argumentCaptor<Toolbar.ActionButton>()
        val size = 48
        val pendingIntent: PendingIntent = mock()
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                actionButtonConfig = CustomTabActionButtonConfig(
                    description = "Button",
                    icon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888),
                    pendingIntent = pendingIntent,
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addActionButton(anyInt(), any())
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        assertEquals(24, (button as ImageButton).drawable.intrinsicHeight)
        assertEquals(24, button.drawable.intrinsicWidth)
    }

    @Test
    fun `GIVEN a wide icon larger than the max drawable size WHEN adding action button to toolbar THEN the icon is scaled to fit`() {
        val captor = argumentCaptor<Toolbar.ActionButton>()
        val width = 96
        val height = 48
        val pendingIntent: PendingIntent = mock()
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                actionButtonConfig = CustomTabActionButtonConfig(
                    description = "Button",
                    icon = Bitmap.createBitmap(IntArray(width * height), width, height, Bitmap.Config.ARGB_8888),
                    pendingIntent = pendingIntent,
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addActionButton(anyInt(), any())
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        assertEquals(24, (button as ImageButton).drawable.intrinsicHeight)
        assertEquals(48, button.drawable.intrinsicWidth)
    }

    @Test
    fun `GIVEN a tall icon larger than the max drawable size WHEN adding action button to toolbar THEN the icon is scaled to fit`() {
        val captor = argumentCaptor<Toolbar.ActionButton>()
        val width = 24
        val height = 48
        val pendingIntent: PendingIntent = mock()
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                actionButtonConfig = CustomTabActionButtonConfig(
                    description = "Button",
                    icon = Bitmap.createBitmap(IntArray(width * height), width, height, Bitmap.Config.ARGB_8888),
                    pendingIntent = pendingIntent,
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addActionButton(anyInt(), any())
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        assertEquals(24, (button as ImageButton).drawable.intrinsicHeight)
        assertEquals(12, button.drawable.intrinsicWidth)
    }

    @Test
    fun `action button uses updated url`() {
        val size = 48
        val pendingIntent: PendingIntent = mock()
        val captor = argumentCaptor<Toolbar.ActionButton>()
        val intentCaptor = argumentCaptor<Intent>()

        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                actionButtonConfig = CustomTabActionButtonConfig(
                    description = "Button",
                    icon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888),
                    pendingIntent = pendingIntent,
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        store.dispatch(
            ContentAction.UpdateUrlAction(
                "mozilla",
                "https://github.com/mozilla-mobile/android-components",
            ),
        ).joinBlocking()

        verify(feature).addActionButton(anyInt(), any())
        verify(toolbar).addBrowserAction(captor.capture())

        doNothing().`when`(pendingIntent).send(any(), anyInt(), any())

        val button = captor.value.createView(FrameLayout(testContext))
        button.performClick()

        verify(pendingIntent).send(any(), anyInt(), intentCaptor.capture())
        assertEquals("https://github.com/mozilla-mobile/android-components", intentCaptor.value.dataString)
    }

    @Test
    fun `initialize calls addMenuItems when config has items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addMenuItems()
    }

    @Test
    fun `initialize calls addMenuItems when menuBuilder has items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
            ) {},
        )

        feature.start()

        verify(feature).addMenuItems()
    }

    @Test
    fun `menu items added WITHOUT current items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder
        assertEquals(1, menuBuilder!!.items.size)
    }

    @Test
    fun `menu items added WITH current items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder
        assertEquals(3, menuBuilder!!.items.size)
    }

    @Test
    fun `menu item added at specified index`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 1,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[1] is SimpleBrowserMenuItem)
    }

    @Test
    fun `menu item added appended if index too large`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[2] is SimpleBrowserMenuItem)
    }

    @Test
    fun `menu item added appended if index too small`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = -4,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[0] is SimpleBrowserMenuItem)
    }

    @Test
    fun `menu item uses updated url`() {
        val pendingIntent: PendingIntent = mock()
        val intentCaptor = argumentCaptor<Intent>()
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", pendingIntent),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        store.dispatch(
            ContentAction.UpdateUrlAction(
                "mozilla",
                "https://github.com/mozilla-mobile/android-components",
            ),
        ).joinBlocking()

        val menuBuilder = toolbar.display.menuBuilder!!

        val item = menuBuilder.items[0]

        val menu: BrowserMenu = mock()
        val view = TextView(testContext)

        item.bind(menu, view)

        view.performClick()

        doNothing().`when`(pendingIntent).send(any(), anyInt(), any())

        verify(pendingIntent).send(any(), anyInt(), intentCaptor.capture())
        assertEquals("https://github.com/mozilla-mobile/android-components", intentCaptor.value.dataString)
    }

    @Test
    fun `onBackPressed removes initialized session`() {
        val store = BrowserStore(
            initialState = BrowserState(
                customTabs = listOf(
                    createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig()),
                ),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeExecuted = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {
                closeExecuted = true
            },
        )

        feature.start()

        val result = feature.onBackPressed()

        assertTrue(result)
        assertTrue(closeExecuted)
    }

    @Test
    fun `onBackPressed without a session does nothing`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeExecuted = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = null,
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {
                closeExecuted = true
            },
        )

        feature.start()

        val result = feature.onBackPressed()

        assertFalse(result)
        assertFalse(closeExecuted)
    }

    @Test
    fun `onBackPressed with uninitialized feature returns false`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeExecuted = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = null,
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {
                closeExecuted = true
            },
        )

        val result = feature.onBackPressed()

        assertFalse(result)
        assertFalse(closeExecuted)
    }

    @Test
    fun `WHEN config toolbar color is dark THEN readableColor is white`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.BLACK),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        verify(feature).updateTheme(
            tab.config.colorSchemes!!.defaultColorSchemeParams!!.toolbarColor,
            tab.config.colorSchemes!!.defaultColorSchemeParams!!.toolbarColor,
            tab.config.colorSchemes!!.defaultColorSchemeParams!!.navigationBarDividerColor,
            Color.WHITE,
        )
        verify(feature).addCloseButton(Color.WHITE, tab.config.closeButtonIcon)
        verify(feature).addActionButton(Color.WHITE, tab.config.actionButtonConfig)
        assertEquals(Color.WHITE, toolbar.display.colors.text)
    }

    @Test
    fun `WHEN config toolbar color is not dark THEN readableColor is black`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = ColorSchemes(
                    defaultColorSchemeParams = ColorSchemeParams(toolbarColor = Color.WHITE),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))

        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        verify(feature).updateTheme(
            tab.config.colorSchemes!!.defaultColorSchemeParams!!.toolbarColor,
            tab.config.colorSchemes!!.defaultColorSchemeParams!!.toolbarColor,
            tab.config.colorSchemes!!.defaultColorSchemeParams!!.navigationBarDividerColor,
            Color.BLACK,
        )
        verify(feature).addCloseButton(Color.BLACK, tab.config.closeButtonIcon)
        verify(feature).addActionButton(Color.BLACK, tab.config.actionButtonConfig)
    }

    @Test
    fun `WHEN config toolbar has no colour set THEN readableColor uses the toolbar display menu colour`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        verify(feature).updateTheme(
            tab.config.colorSchemes?.defaultColorSchemeParams?.toolbarColor,
            tab.config.colorSchemes?.defaultColorSchemeParams?.toolbarColor,
            tab.config.colorSchemes?.defaultColorSchemeParams?.navigationBarDividerColor,
            toolbar.display.colors.menu,
        )
        verify(feature).addCloseButton(toolbar.display.colors.menu, tab.config.closeButtonIcon)
        verify(feature).addActionButton(toolbar.display.colors.menu, tab.config.actionButtonConfig)
        assertEquals(Color.WHITE, toolbar.display.colors.menu)
    }

    @Test
    fun `GIVEN the close button has enabled customization WHEN needing to show the close button THEN use the provided icon`() {
        val customCloseIcon: Bitmap = mock()
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                closeButtonIcon = customCloseIcon,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
            ) {},
        )

        feature.start()

        verify(feature).addCloseButton(toolbar.display.colors.menu, customCloseIcon)
    }

    @Test
    fun `GIVEN the close button has disabled customization WHEN needing to show the close button THEN use the default icon`() {
        val customCloseIcon: Bitmap = mock()
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                closeButtonIcon = customCloseIcon,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                customTabsToolbarButtonConfig = CustomTabsToolbarButtonConfig(
                    allowCustomizingCloseButton = false,
                ),
            ) {},
        )

        feature.start()

        verify(feature).addCloseButton(toolbar.display.colors.menu, null)
    }

    @Test
    fun `WHEN tab is private THEN readableColor is the default private color`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(showShareMenuItem = true),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store = store,
                toolbar = toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
                customTabsColorsConfig = getPrivateCustomTabColorsConfig(),
            ) {},
        )

        feature.start()

        val colorResId = testContext.theme.resolveAttribute(android.R.attr.textColorPrimary)
        val privateColor = getColor(testContext, colorResId)
        verify(feature).addCloseButton(privateColor, tab.config.closeButtonIcon)
        verify(feature).addActionButton(privateColor, tab.config.actionButtonConfig)
        verify(feature).addShareButton(privateColor)
    }

    @Test
    fun `show title only if not empty`() {
        val dispatcher = UnconfinedTestDispatcher()
        Dispatchers.setMain(dispatcher)

        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(),
            title = "",
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        assertEquals("", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction(
                "mozilla",
                "Internet for people, not profit - Mozilla",
            ),
        ).joinBlocking()

        assertEquals("Internet for people, not profit - Mozilla", toolbar.title)

        Dispatchers.resetMain()
    }

    @Test
    fun `Will use URL as title if title was shown once and is now empty`() {
        val dispatcher = UnconfinedTestDispatcher()
        Dispatchers.setMain(dispatcher)

        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(),
            title = "",
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        feature.start()

        assertEquals("", toolbar.title)

        store.dispatch(
            ContentAction.UpdateUrlAction("mozilla", "https://www.mozilla.org/en-US/firefox/"),
        ).joinBlocking()

        assertEquals("", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction(
                "mozilla",
                "Firefox - Protect your life online with privacy-first products",
            ),
        ).joinBlocking()

        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        store.dispatch(
            ContentAction.UpdateUrlAction("mozilla", "https://github.com/mozilla-mobile/android-components"),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", "Le GitHub"),
        ).joinBlocking()

        assertEquals("Le GitHub", toolbar.title)

        store.dispatch(
            ContentAction.UpdateUrlAction("mozilla", "https://github.com/mozilla-mobile/fenix"),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/fenix", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", ""),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/fenix", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction(
                "mozilla",
                "A collection of Android libraries to build browsers or browser-like applications.",
            ),
        ).joinBlocking()

        assertEquals("A collection of Android libraries to build browsers or browser-like applications.", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", ""),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/fenix", toolbar.title)
    }

    private fun extractActionView(
        browserToolbar: BrowserToolbar,
        contentDescription: String,
    ): ImageButton? {
        var actionView: ImageButton? = null

        browserToolbar.forEach { group ->
            val viewGroup = group as ViewGroup

            viewGroup.forEach inner@{ subGroup ->
                if (subGroup is ViewGroup) {
                    subGroup.forEach {
                        if (it is ImageButton && it.contentDescription == contentDescription) {
                            actionView = it
                            return@inner
                        }
                    }
                }
            }
        }

        return actionView
    }

    private fun getPrivateCustomTabColorsConfig() = CustomTabsColorsConfig(
        updateToolbarsColor = false,
        updateStatusBarColor = false,
        updateSystemNavigationBarColor = false,
    )
}
