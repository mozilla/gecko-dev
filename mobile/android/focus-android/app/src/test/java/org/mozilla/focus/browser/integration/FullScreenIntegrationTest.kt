/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.app.Activity
import android.content.res.Resources
import android.view.View
import android.view.Window
import android.view.WindowInsetsController
import android.view.WindowManager
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.isVisible
import mozilla.components.browser.engine.gecko.GeckoEngineView
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.prompts.dialog.FullScreenNotification
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.focus.ext.disableDynamicBehavior
import org.mozilla.focus.ext.enableDynamicBehavior
import org.mozilla.focus.ext.hide
import org.mozilla.focus.ext.showAsFixed
import org.mozilla.focus.utils.Settings
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
internal class FullScreenIntegrationTest {
    @Test
    fun `WHEN the integration is started THEN start FullScreenFeature`() {
        val feature: FullScreenFeature = mock()
        val integration = createFullScreenIntegration().apply {
            this.feature = feature
        }

        integration.start()

        verify(feature).start()
    }

    @Test
    fun `WHEN the integration is stopped THEN stop FullScreenFeature`() {
        val feature: FullScreenFeature = mock()
        val integration = createFullScreenIntegration().apply {
            this.feature = feature
        }

        integration.stop()

        verify(feature).stop()
    }

    @Test
    fun `WHEN back is pressed THEN send this to the feature`() {
        val feature: FullScreenFeature = mock()
        val integration = createFullScreenIntegration().apply {
            this.feature = feature
        }

        integration.onBackPressed()

        verify(feature).onBackPressed()
    }

    @Test
    fun `WHEN the viewport changes THEN update layoutInDisplayCutoutMode`() {
        val windowAttributes = WindowManager.LayoutParams()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(windowAttributes).`when`(activityWindow).attributes
        val integration = createFullScreenIntegration(activity = activity)

        integration.viewportFitChanged(33)

        assertEquals(33, windowAttributes.layoutInDisplayCutoutMode)
    }

    @Test
    @Suppress("DEPRECATION")
    @Config(sdk = [28])
    fun `WHEN entering immersive mode THEN hide all system bars in SDK 28`() {
        val decorView: View = mock()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        val layoutParams: WindowManager.LayoutParams = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(layoutParams).`when`(activityWindow).attributes

        val integration = createFullScreenIntegration(activity = activity)

        integration.switchToImmersiveMode()

        // verify entering immersive mode
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        // verify that the immersive mode restoration is set as expected
        verify(decorView).setOnApplyWindowInsetsListener(any())
    }

    @Test
    fun `WHEN entering immersive mode THEN hide all system bars`() {
        val decorView: View = mock()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        val layoutParams = WindowManager.LayoutParams()
        val insetsController: WindowInsetsController = mock()

        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(layoutParams).`when`(activityWindow).attributes
        doReturn(insetsController).`when`(activityWindow).insetsController

        val integration = createFullScreenIntegration(activity = activity)

        integration.switchToImmersiveMode()

        // verify hiding system bars
        verify(insetsController).hide(WindowInsetsCompat.Type.systemBars())

        verify(activityWindow).setFlags(
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
        )

        assertEquals(
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
            layoutParams.layoutInDisplayCutoutMode,
        )
    }

    @Test
    @Suppress("DEPRECATION")
    @Config(sdk = [28])
    fun `GIVEN immersive mode WHEN exitImmersiveModeIfNeeded is called THEN show the system bars on SDK 28`() {
        val decorView: View = mock()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        val layoutParams: WindowManager.LayoutParams = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(layoutParams).`when`(activityWindow).attributes
        val integration = createFullScreenIntegration(activity = activity)

        integration.exitImmersiveMode()
        // Hiding the system bar hides the status and navigation bars.
        // setSystemUiVisibility will be called twice by WindowInsetsControllerCompat
        // once for setting the status bar and another for setting the navigation bar
        verify(decorView, times(2)).systemUiVisibility
        verify(decorView).setOnApplyWindowInsetsListener(null)
    }

    @Test
    fun `GIVEN immersive mode WHEN exitImmersiveModeIfNeeded is called THEN show the system bars`() {
        val decorView: View = mock()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        val layoutParams = WindowManager.LayoutParams()
        val insetsController: WindowInsetsController = mock()

        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(layoutParams).`when`(activityWindow).attributes
        doReturn(insetsController).`when`(activityWindow).insetsController

        val integration = createFullScreenIntegration(activity = activity)

        integration.exitImmersiveMode()

        verify(insetsController).show(WindowInsetsCompat.Type.systemBars())
        verify(decorView).setOnApplyWindowInsetsListener(null)
        verify(activityWindow).clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)

        assertEquals(
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
            layoutParams.layoutInDisplayCutoutMode,
        )
    }

    @Test
    fun `GIVEN a11y is enabled WHEN enterBrowserFullscreen THEN hide the toolbar`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val settings: Settings = mock()
        doReturn(true).`when`(settings).isAccessibilityEnabled()
        val integration = createFullScreenIntegration(
            settings = settings,
            toolbar = toolbar,
            engineView = engineView,
        )

        integration.enterBrowserFullscreen()

        verify(toolbar).hide(engineView)
        verify(toolbar, never()).collapse()
        verify(toolbar, never()).disableDynamicBehavior(engineView)
    }

    @Test
    fun `GIVEN a11y is disabled WHEN enterBrowserFullscreen THEN collapse and disable the dynamic toolbar`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val integration = createFullScreenIntegration(
            settings = settings,
            toolbar = toolbar,
            engineView = engineView,
        )

        integration.enterBrowserFullscreen()

        verify(toolbar, never()).hide(engineView)
        with(inOrder(toolbar)) {
            verify(toolbar).collapse()
            verify(toolbar).disableDynamicBehavior(engineView)
        }
    }

    @Test
    fun `GIVEN a11y is enabled WHEN exitBrowserFullscreen THEN show the toolbar`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val settings: Settings = mock()
        doReturn(true).`when`(settings).isAccessibilityEnabled()
        val resources: Resources = mock()
        val activity: Activity = mock()
        doReturn(resources).`when`(activity).resources
        val integration =
            createFullScreenIntegration(
                activity = activity,
                settings = settings,
                toolbar = toolbar,
                engineView = engineView,
            )

        integration.exitBrowserFullscreen()

        verify(toolbar).showAsFixed(activity, engineView)
        verify(toolbar, never()).expand()
        verify(toolbar, never()).enableDynamicBehavior(activity, engineView)
    }

    @Test
    fun `GIVEN a11y is disabled WHEN exitBrowserFullscreen THEN enable the dynamic toolbar and expand it`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val resources: Resources = mock()
        val activity: Activity = mock()
        doReturn(resources).`when`(activity).resources
        val integration =
            createFullScreenIntegration(
                activity = activity,
                settings = settings,
                toolbar = toolbar,
                engineView = engineView,
            )

        integration.exitBrowserFullscreen()

        verify(toolbar, never()).showAsFixed(activity, engineView)
        with(inOrder(toolbar)) {
            verify(toolbar).enableDynamicBehavior(activity, engineView)
            verify(toolbar).expand()
        }
    }

    @Test
    fun `WHEN entering fullscreen THEN put browser in fullscreen, hide system bars and enter immersive mode`() {
        val toolbar = BrowserToolbar(testContext)
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val activity = Robolectric.buildActivity(Activity::class.java).get()
        val statusBar: View = mock()
        val integration = spy(
            createFullScreenIntegration(
                activity = activity,
                settings = settings,
                toolbar = toolbar,
                statusBar = statusBar,
                engineView = engineView,
            ),
        )

        val fullScreenNotification = mock<FullScreenNotification>()
        integration.fullScreenChanged(true, fullScreenNotification)

        verify(integration).enterBrowserFullscreen()
        verify(statusBar).isVisible = false
        verify(fullScreenNotification).show()
        verify(integration).switchToImmersiveMode()
    }

    @Test
    @Config(sdk = [28])
    fun `WHEN exiting fullscreen THEN put browser in fullscreen, hide system bars and enter immersive mode in SDK 28`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val resources: Resources = mock()
        val activityWindow: Window = mock()
        val decorView: View = mock()
        val windowAttributes = WindowManager.LayoutParams()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(windowAttributes).`when`(activityWindow).attributes
        doReturn(resources).`when`(activity).resources
        doReturn("").`when`(resources).getString(anyInt())
        val statusBar: View = mock()
        val integration = spy(
            createFullScreenIntegration(
                activity = activity,
                settings = settings,
                toolbar = toolbar,
                statusBar = statusBar,
                engineView = engineView,
            ),
        )

        integration.fullScreenChanged(false)

        verify(integration).exitBrowserFullscreen()
        verify(integration).exitImmersiveMode()
        verify(statusBar).isVisible = true
    }

    @Test
    fun `WHEN exiting fullscreen THEN put browser in fullscreen, hide system bars and enter immersive mode in`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()

        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()

        val resources: Resources = mock()
        val activityWindow: Window = mock()
        val decorView: View = mock()
        val windowAttributes = WindowManager.LayoutParams()
        val activity: Activity = mock()
        val insetsController: WindowInsetsController = mock()

        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(windowAttributes).`when`(activityWindow).attributes
        doReturn(resources).`when`(activity).resources
        doReturn("").`when`(resources).getString(anyInt())
        doReturn(insetsController).`when`(activityWindow).insetsController

        val statusBar: View = mock()
        val integration = spy(
            createFullScreenIntegration(
                activity = activity,
                settings = settings,
                toolbar = toolbar,
                statusBar = statusBar,
                engineView = engineView,
            ),
        )

        integration.fullScreenChanged(false)

        verify(integration).exitBrowserFullscreen()
        verify(integration).exitImmersiveMode()
        verify(statusBar).isVisible = true
    }

    private fun createFullScreenIntegration(
        activity: Activity = mock(),
        settings: Settings = mock(),
        toolbar: BrowserToolbar = mock(),
        statusBar: View = mock(),
        engineView: GeckoEngineView = mock(),
    ) = FullScreenIntegration(
        activity = activity,
        store = mock(),
        tabId = null,
        sessionUseCases = mock(),
        settings = settings,
        toolbarView = toolbar,
        statusBar = statusBar,
        engineView = engineView,
    )
}
