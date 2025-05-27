/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.graphics.Color
import android.text.SpannableStringBuilder
import android.text.SpannableStringBuilder.SPAN_INCLUSIVE_INCLUSIVE
import android.text.style.ForegroundColorSpan
import android.view.View
import androidx.annotation.ColorInt
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class OriginViewTest {

    private fun SpannableStringBuilder.applyUrlColors(
        @ColorInt urlColor: Int,
        @ColorInt registrableDomainColor: Int,
        registrableDomainOrHostSpan: Pair<Int, Int>,
    ): SpannableStringBuilder = apply {
        setSpan(
            ForegroundColorSpan(urlColor),
            0,
            length,
            SPAN_INCLUSIVE_INCLUSIVE,
        )

        val (start, end) = registrableDomainOrHostSpan
        setSpan(
            Toolbar.RegistrableDomainColorSpan(registrableDomainColor),
            start,
            end,
            SPAN_INCLUSIVE_INCLUSIVE,
        )
    }

    @Test
    fun `scrollToShowRegistrableDomain scrolls when domain exceeds width`() {
        val view = spy(OriginView(testContext))
        val url = "https://www.really-long-example-domain.com/"
        val spannedUrl = SpannableStringBuilder(url).apply {
            applyUrlColors(
                urlColor = Color.GREEN,
                registrableDomainColor = Color.RED,
                registrableDomainOrHostSpan = 8 to 42,
            )
        }

        // Long domain wouldn't fit in the view
        doReturn(500f).`when`(view).measureUrlTextWidh(any())
        view.urlView.layout(0, 0, 200, 100)

        view.url = spannedUrl

        assertEquals(300, view.urlView.scrollX)
    }

    @Test
    fun `scrollToShowRegistrableDomain does not scroll when domain fits in view`() {
        val view = spy(OriginView(testContext))
        val url = "https://mozilla.org/"
        val spannedUrl = SpannableStringBuilder(url).apply {
            applyUrlColors(
                urlColor = Color.GREEN,
                registrableDomainColor = Color.RED,
                registrableDomainOrHostSpan = 8 to 19,
            )
        }

        doReturn(50f).`when`(view).measureUrlTextWidh(any())
        view.urlView.layout(0, 0, 200, 100)

        view.url = spannedUrl

        assertEquals(0, view.urlView.scrollX)
    }

    @Test
    fun `scrollToShowRegistrableDomain does not scroll when no span exists`() {
        val view = OriginView(testContext)

        val spanned = SpannableStringBuilder("nospan.com") // no span set

        view.measure(0, 0)
        view.layout(0, 0, 500, 100)

        view.url = spanned

        assertEquals(0, view.urlView.scrollX)
    }

    @Test
    fun `URL text direction is always LTR`() {
        val originView = OriginView(testContext)
        originView.url = "ختار.ار/www.mozilla.org/1"
        assertEquals(View.TEXT_DIRECTION_LTR, originView.urlView.textDirection)
        assertEquals(View.LAYOUT_DIRECTION_LTR, originView.urlView.layoutDirection)
    }
}
