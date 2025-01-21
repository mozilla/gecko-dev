/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.content.Context
import android.graphics.* // ktlint-disable no-wildcard-imports
import android.graphics.Bitmap
import android.view.inputmethod.InputMethodManager
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoView
import org.mozilla.geckoview.PanZoomController
import org.mozilla.geckoview.ScreenLength
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.util.AssertUtils

@RunWith(AndroidJUnit4::class)
@MediumTest
class InteractiveWidgetTest : BaseSessionTest() {
    private val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)
    private val dynamicToolbarMaxHeight = 100
    private lateinit var imm: InputMethodManager
    private lateinit var view: GeckoView

    @get:Rule
    override val rules: RuleChain = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity { activity ->
            // To make OnApplyWindowInsetsListener work for `interactive-widget` features
            // we need to setSession explicitly.
            activity.view.setSession(mainSession)
            activity.view.setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight)
            imm = activity.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            view = activity.view as GeckoView
        }
    }

    @After
    fun cleanup() {
        try {
            activityRule.scenario.onActivity { activity ->
                activity.view.releaseSession()
            }
        } catch (e: Exception) {}
    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    @Test
    fun stickyElementWithDynamicToolbarOnResizesVisual() {
        mainSession.setActive(true)

        mainSession.loadTestPath(BaseSessionTest.INTERACTIVE_WIDGET_HTML_PATH)
        mainSession.waitForPageStop()
        mainSession.promiseAllPaintsDone()
        mainSession.flushApzRepaints()

        view.requestFocus()

        var promise = mainSession.evaluatePromiseJS(
            """
              new Promise(resolve => {
                visualViewport.addEventListener('resize', () => {
                  resolve(true);
                }, { once: true });
              });
            """.trimIndent(),
        )
        // Explicitly call `waitForRoundTrip()` to make sure the above event listener
        // has set up in the content.
        mainSession.waitForRoundTrip()

        // Open the software keyboard.
        imm.showSoftInput(view, 0)

        assertThat(
            "The visual viewport height should be changed in response to the the keyboard showing",
            promise.value as Boolean,
            equalTo(true),
        )

        // Hide the dynamic toolbar.
        view.setVerticalClipping(-dynamicToolbarMaxHeight)

        // To make sure the dynamic toolbar height has been reflected into APZ.
        mainSession.flushApzRepaints()
        // Also to make sure the dynamic toolbar height has been reflected on the main-thread.
        mainSession.promiseAllPaintsDone()

        // Scroll the visual viewport to the bottom.
        mainSession.panZoomController.scrollTo(
            ScreenLength.zero(),
            ScreenLength.bottom(),
            PanZoomController.SCROLL_BEHAVIOR_AUTO,
        )
        mainSession.flushApzRepaints()
        mainSession.promiseAllPaintsDone()

        fun createReferenceImage(height: Double): Bitmap {
            val rect = Rect()
            mainSession.getSurfaceBounds(rect)

            val bitmap = Bitmap.createBitmap(rect.width(), height.toInt(), Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bitmap)
            val paint = Paint()
            paint.color = Color.rgb(255, 255, 255)
            canvas.drawRect(0f, 0f, rect.width().toFloat(), height.toFloat(), paint)

            // Draw the sticky element area.
            paint.color = Color.rgb(0, 128, 0)
            canvas.drawRect(
                0f,
                (height - dynamicToolbarMaxHeight).toFloat(),
                rect.width().toFloat(),
                height.toFloat(),
                paint,
            )
            return bitmap
        }

        val height = mainSession.evaluateJS("window.visualViewport.height") as Double
        val pixelRatio = mainSession.evaluateJS("window.devicePixelRatio") as Double
        val reference = createReferenceImage(height * pixelRatio)

        val result = sessionRule.waitForResult(view.capturePixels())
        AssertUtils.assertScreenshotResult(result, reference)

        // Close the software keyboard.
        imm.hideSoftInputFromWindow(view.getWindowToken(), 0)
    }
}
