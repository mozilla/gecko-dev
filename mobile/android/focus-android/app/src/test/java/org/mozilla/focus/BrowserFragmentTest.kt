/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.content.Context
import android.graphics.Bitmap
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mozilla.focus.databinding.FragmentBrowserBinding
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.widget.ResizableKeyboardCoordinatorLayout
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserFragmentTest {
    @Test
    fun testEngineViewInflationAndParentInteraction() {
        val layoutInflater = LayoutInflater.from(testContext)

        // Intercept the inflation process
        layoutInflater.factory2 = object : LayoutInflater.Factory2 {
            override fun onCreateView(
                parent: View?,
                name: String,
                context: Context,
                attrs: AttributeSet,
            ): View? {
                // Inflate a DummyEngineView when trying to inflate an EngineView
                if (name == EngineView::class.java.name) {
                    return DummyEngineView(testContext)
                }

                // For other types of views, let the system handle it
                return null
            }

            override fun onCreateView(name: String, context: Context, attrs: AttributeSet): View? {
                return onCreateView(null, name, context, attrs)
            }
        }

        val binding = FragmentBrowserBinding.inflate(LayoutInflater.from(testContext))
        val engineView: EngineView = binding.engineView

        assertNotNull(engineView)

        // Get the layout parent of the EngineView
        val engineViewParent = spy(
            (engineView as View).parent as ResizableKeyboardCoordinatorLayout,
        )

        assertNotNull(engineViewParent)

        engineViewParent.requestDisallowInterceptTouchEvent(true)

        // Verify that the EngineView's parent does not propagate requestDisallowInterceptTouchEvent
        verify(engineViewParent).requestDisallowInterceptTouchEvent(true)
        // If propagated, an additional ViewGroup.requestDisallowInterceptTouchEvent would have been registered.
        verifyNoMoreInteractions(engineViewParent)
    }

    @Test
    fun `test isOnboardingTab returns the expected value for each external app type`() {
        ExternalAppType.entries.forEach {
            val sessionState = testCustomTabSessionState(it)
            when (it) {
                ExternalAppType.CUSTOM_TAB,
                ExternalAppType.PROGRESSIVE_WEB_APP,
                ExternalAppType.TRUSTED_WEB_ACTIVITY,
                -> assertFalse(BrowserFragment().isOnboardingTab(sessionState))

                ExternalAppType.ONBOARDING_CUSTOM_TAB ->
                    assertTrue(
                        BrowserFragment().isOnboardingTab(sessionState),
                    )
            }
        }
    }

    private fun testCustomTabSessionState(externalAppType: ExternalAppType) = CustomTabSessionState(
        content = ContentState(""),
        config = CustomTabConfig(externalAppType = externalAppType),
    )
}

/**
 * Dummy implementation of the EngineView interface.
 */
class DummyEngineView(context: Context) : View(context), EngineView {
    init {
        id = R.id.engineView
    }

    override fun render(session: EngineSession) {
        // no-op
    }

    override fun release() {
        // no-op
    }

    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) {
        // no-op
    }

    override fun setVerticalClipping(clippingHeight: Int) {
        // no-op
    }

    override fun setDynamicToolbarMaxHeight(height: Int) {
        // no-op
    }

    override fun setActivityContext(context: Context?) {
        // no-op
    }

    override var selectionActionDelegate: SelectionActionDelegate? = null

    override fun addWindowInsetsListener(
        key: String,
        listener: androidx.core.view.OnApplyWindowInsetsListener?,
    ) {
        // no-op
    }

    override fun removeWindowInsetsListener(key: String) {
        // no-op
    }
}
