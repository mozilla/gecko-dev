/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar.internal

import android.graphics.Color
import android.net.InetAddresses
import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan
import android.util.Patterns
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements

@RunWith(AndroidJUnit4::class)
@Config(shadows = [ShadowInetAddresses::class])
class URLRendererTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `Lifecycle methods start and stop job`() {
        val renderer = URLRenderer(mock(), mock())

        assertNull(renderer.job)

        renderer.start()

        assertNotNull(renderer.job)
        assertTrue(renderer.job!!.isActive)

        renderer.stop()

        assertNotNull(renderer.job)
        assertFalse(renderer.job!!.isActive)
    }

    @Test
    fun `Render with configuration`() {
        runTestOnMain {
            val configuration = ToolbarFeature.UrlRenderConfiguration(
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
                registrableDomainColor = Color.RED,
                urlColor = Color.GREEN,
            )

            val toolbar: Toolbar = mock()

            val renderer = URLRenderer(toolbar, configuration)

            renderer.updateUrl("https://www.mozilla.org/")

            val captor = argumentCaptor<CharSequence>()
            verify(toolbar).url = captor.capture()

            assertNotNull(captor.value)
            assertTrue(captor.value is SpannableStringBuilder)
            val url = captor.value as SpannableStringBuilder

            assertEquals("https://www.mozilla.org/", url.toString())

            val spans = url.getSpans(0, url.length, ForegroundColorSpan::class.java)

            assertEquals(2, spans.size)
            assertEquals(Color.GREEN, spans[0].foregroundColor)
            assertEquals(Color.RED, spans[1].foregroundColor)

            val domain = url.subSequence(12, 23)
            assertEquals("mozilla.org", domain.toString())

            val domainSpans = url.getSpans(13, 23, ForegroundColorSpan::class.java)
            assertEquals(2, domainSpans.size)
            assertEquals(Color.GREEN, domainSpans[0].foregroundColor)
            assertEquals(Color.RED, domainSpans[1].foregroundColor)

            val prefix = url.subSequence(0, 12)
            assertEquals("https://www.", prefix.toString())

            val prefixSpans = url.getSpans(0, 12, ForegroundColorSpan::class.java)
            assertEquals(1, prefixSpans.size)
            assertEquals(Color.GREEN, prefixSpans[0].foregroundColor)

            val suffix = url.subSequence(23, url.length)
            assertEquals("/", suffix.toString())

            val suffixSpans = url.getSpans(23, url.length, ForegroundColorSpan::class.java)
            assertEquals(1, suffixSpans.size)
            assertEquals(Color.GREEN, suffixSpans[0].foregroundColor)
        }
    }

    private suspend fun getSpannedUrl(testUrl: String): SpannableStringBuilder {
        val configuration = ToolbarFeature.UrlRenderConfiguration(
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            registrableDomainColor = Color.RED,
            urlColor = Color.GREEN,
            renderStyle = ToolbarFeature.RenderStyle.ColoredUrl,
        )

        val toolbar: Toolbar = mock()

        val renderer = URLRenderer(toolbar, configuration)

        renderer.updateUrl(testUrl)

        val captor = argumentCaptor<CharSequence>()
        verify(toolbar).url = captor.capture()

        return requireNotNull(captor.value as? SpannableStringBuilder) { "Toolbar URL should not be null" }
    }

    private suspend fun testRenderWithColoredUrl(
        testUrl: String,
        expectedRegistrableDomainSpan: Pair<Int, Int>,
    ) {
        val url = getSpannedUrl(testUrl)

        assertEquals(testUrl, url.toString())

        val spans = url.getSpans(0, url.length, ForegroundColorSpan::class.java)

        assertEquals(2, spans.size)
        assertEquals(Color.GREEN, spans[0].foregroundColor)
        assertEquals(Color.RED, spans[1].foregroundColor)

        assertEquals(0, url.getSpanStart(spans[0]))
        assertEquals(testUrl.length, url.getSpanEnd(spans[0]))

        assertEquals(expectedRegistrableDomainSpan.first, url.getSpanStart(spans[1]))
        assertEquals(expectedRegistrableDomainSpan.second, url.getSpanEnd(spans[1]))
    }

    private suspend fun testRenderWithUncoloredUrl(testUrl: String) {
        val url = getSpannedUrl(testUrl)

        assertEquals(testUrl, url.toString())

        val spans = url.getSpans(0, url.length, ForegroundColorSpan::class.java)

        assertEquals(0, spans.size)
    }

    @Test
    fun `GIVEN a simple domain WHEN getting registrable domain span in host THEN span is returned`() {
        runTestOnMain {
            val domainSpan = getRegistrableDomainSpanInHost(
                host = "www.mozilla.org",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(4 to 15, domainSpan)
        }
    }

    @Test
    fun `GIVEN a host with a trailing period in the domain WHEN getting registrable domain span in host THEN span is returned`() {
        runTestOnMain {
            val domainSpan = getRegistrableDomainSpanInHost(
                host = "www.mozilla.org.",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(4 to 15, domainSpan)
        }
    }

    @Test
    fun `GIVEN a host with a repeated domain WHEN getting registrable domain span in host THEN the span of the last occurrence of domain is returned`() {
        runTestOnMain {
            val domainSpan = getRegistrableDomainSpanInHost(
                host = "mozilla.org.mozilla.org",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(12 to 23, domainSpan)
        }
    }

    @Test
    fun `GIVEN an IPv4 address as host WHEN getting registrable domain span in host THEN null is returned`() {
        runTestOnMain {
            val domainSpan = getRegistrableDomainSpanInHost(
                host = "127.0.0.1",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertNull(domainSpan)
        }
    }

    @Test
    fun `GIVEN an IPv6 address as host WHEN getting registrable domain span in host THEN null is returned`() {
        runTestOnMain {
            val domainSpan = getRegistrableDomainSpanInHost(
                host = "[::1]",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertNull(domainSpan)
        }
    }

    @Test
    fun `GIVEN a non PSL domain as host WHEN getting registrable domain span in host THEN null is returned`() {
        runTestOnMain {
            val domainSpan = getRegistrableDomainSpanInHost(
                host = "localhost",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertNull(domainSpan)
        }
    }

    @Test
    fun `GIVEN a simple URL WHEN getting registrable domain or host span THEN span is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "https://www.mozilla.org/",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(12 to 23, span)
        }
    }

    @Test
    fun `GIVEN a URL with a trailing period in the domain WHEN getting registrable domain or host span THEN span is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "https://www.mozilla.org./",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(12 to 23, span)
        }
    }

    @Test
    fun `GIVEN a URL with a repeated domain WHEN getting registrable domain or host span THEN the span of the last occurrence of domain is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "https://mozilla.org.mozilla.org/",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(20 to 31, span)
        }
    }

    @Test
    fun `GIVEN a URL with an IPv4 address WHEN getting registrable domain or host span THEN the span of the IP part is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "http://127.0.0.1/",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(7 to 16, span)
        }
    }

    @Test
    fun `GIVEN a URL with an IPv6 address WHEN getting registrable domain or host span THEN the span of the IP part is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "http://[::1]/",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(7 to 12, span)
        }
    }

    @Test
    fun `GIVEN a URL with a non PSL domain WHEN getting registrable domain or host span THEN the span of the host part is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "http://localhost/",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertEquals(7 to 16, span)
        }
    }

    @Test
    fun `GIVEN an internal page name WHEN getting registrable domain or host span THEN null is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "about:mozilla",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertNull(span)
        }
    }

    @Test
    fun `GIVEN a content URI WHEN getting registrable domain or host span THEN null is returned`() {
        runTestOnMain {
            val span = getRegistrableDomainOrHostSpan(
                url = "content://media/external/file/1000000000",
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
            )

            assertNull(span)
        }
    }

    @Test
    fun `GIVEN a simple URL WHEN rendering it THEN registrable domain is colored`() {
        runTestOnMain {
            testRenderWithColoredUrl(
                testUrl = "https://www.mozilla.org/",
                expectedRegistrableDomainSpan = 12 to 23,
            )
        }
    }

    @Test
    fun `GIVEN a URL with a trailing period in the domain WHEN rendering it THEN registrable domain is colored`() {
        runTestOnMain {
            testRenderWithColoredUrl(
                testUrl = "https://www.mozilla.org./",
                expectedRegistrableDomainSpan = 12 to 23,
            )
        }
    }

    @Test
    fun `GIVEN a URL with a repeated domain WHEN rendering it THEN the last occurrence of domain is colored`() {
        runTestOnMain {
            testRenderWithColoredUrl(
                testUrl = "https://mozilla.org.mozilla.org/",
                expectedRegistrableDomainSpan = 20 to 31,
            )
        }
    }

    @Test
    fun `GIVEN a URL with an IPv4 address WHEN rendering it THEN the IP part is colored`() {
        runTestOnMain {
            testRenderWithColoredUrl(
                testUrl = "http://127.0.0.1/",
                expectedRegistrableDomainSpan = 7 to 16,
            )
        }
    }

    @Test
    fun `GIVEN a URL with an IPv6 address WHEN rendering it THEN the IP part is colored`() {
        runTestOnMain {
            testRenderWithColoredUrl(
                testUrl = "http://[::1]/",
                expectedRegistrableDomainSpan = 7 to 12,
            )
        }
    }

    @Test
    fun `GIVEN a URL with a non PSL domain WHEN rendering it THEN host colored`() {
        runTestOnMain {
            testRenderWithColoredUrl(
                testUrl = "http://localhost/",
                expectedRegistrableDomainSpan = 7 to 16,
            )
        }
    }

    @Test
    fun `GIVEN an internal page name WHEN rendering it THEN nothing is colored`() {
        runTestOnMain {
            testRenderWithUncoloredUrl("about:mozilla")
        }
    }

    @Test
    fun `GIVEN a content URI WHEN rendering it THEN nothing is colored`() {
        runTestOnMain {
            testRenderWithUncoloredUrl("content://media/external/file/1000000000")
        }
    }
}

/**
 * Robolectric default implementation of [InetAddresses] returns false for any address.
 * This shadow is used to override that behavior and return true for any IP address.
 */
@Implements(InetAddresses::class)
class ShadowInetAddresses {
    companion object {
        @Implementation
        @JvmStatic
        @Suppress("DEPRECATION")
        fun isNumericAddress(address: String): Boolean {
            return Patterns.IP_ADDRESS.matcher(address).matches() || address.contains(":")
        }
    }
}
