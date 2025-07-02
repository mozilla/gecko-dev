/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import android.net.InetAddresses
import android.util.Patterns
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.runTest
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements

@RunWith(RobolectricTestRunner::class)
@Config(shadows = [ShadowInetAddresses::class])
class DomainIndexesInUrlTest {
    @Test
    fun `GIVEN a simple URL WHEN getting the domain indexes THEN get the start and end indexes of the domain in URL`() =
        testDomainIndexesInURL(
            url = "https://www.mozilla.org/",
            expectedIndexes = 12 to 23,
        )

    @Test
    fun `GIVEN a URL with a trailing period in the domain WHEN getting the domain indexes THEN get the start and end indexes of the domain in URL without the period`() =
        testDomainIndexesInURL(
            url = "https://www.mozilla.org./",
            expectedIndexes = 12 to 23,
        )

    @Test
    fun `GIVEN a URL with a repeated domain WHEN getting the domain indexes THEN get the start and end indexes of the last domain string occurrence in URL`() =
        testDomainIndexesInURL(
            url = "https://mozilla.org.mozilla.org/",
            expectedIndexes = 20 to 31,
        )

    @Test
    fun `GIVEN a URL with a long subdomain and page WHEN getting the domain indexes THEN get the start and end indexes of the domain in URL`() =
        testDomainIndexesInURL(
            url = "https://firefox-is-an-awesome-browser.mozilla.org/based_on_a_recent_british_study",
            expectedIndexes = 38 to 49,
        )

    @Test
    fun `GIVEN a URL with an IPv4 address WHEN getting the domain indexes THEN get the start and end indexes of the IP string in URL`() =
        testDomainIndexesInURL(
            url = "http://127.0.0.1/",
            expectedIndexes = 7 to 16,
        )

    @Test
    fun `GIVEN a URL with an IPv6 address WHEN getting the domain indexes THEN get the start and end indexes of the IP string in URL`() =
        testDomainIndexesInURL(
            url = "http://[::1]/",
            expectedIndexes = 7 to 12,
        )

    @Test
    fun `GIVEN a URL with a non PSL domain WHEN getting the domain indexes THEN get the start and end indexes of the domain in URL`() =
        testDomainIndexesInURL(
            url = "http://localhost/",
            expectedIndexes = 7 to 16,
        )

    @Test
    fun `GIVEN an internal page name WHEN getting the domain indexes THEN return a null value`() =
        testDomainIndexesInURL(
            url = "about:mozilla",
            expectedIndexes = null,
        )

    @Test
    fun `GIVEN a content URI WHEN getting the domain indexes THEN return a null value`() =
        testDomainIndexesInURL(
            url = "content://media/external/file/1000000000",
            expectedIndexes = null,
        )

    @Test
    fun `GIVEN a blob URI WHEN getting the domain indexes THEN get the start and end indexes of the domain in URL`() =
        testDomainIndexesInURL(
            url = "blob:https://www.mozilla.org/69a29afb-938c-4b9e-9fca-b2f79755047a",
            expectedIndexes = 17 to 28,
        )

    @Test
    fun `GIVEN a blob URI with duplicated blob prefix WHEN getting the domain indexes THEN return a null value`() =
        testDomainIndexesInURL(
            url = "blob:blob:https://www.mozilla.org/69a29afb-938c-4b9e-9fca-b2f79755047a",
            expectedIndexes = null,
        )

    @Test
    fun `GIVEN a URL with duplicated www subdomain and domain WHEN getting the domain indexes THEN get the start and end indexes of the domain in URL`() =
        testDomainIndexesInURL(
            url = "https://www.www",
            expectedIndexes = 8 to 15,
        )

    @Test
    fun `GIVEN an invalid www www URL WHEN getting the domain indexes THEN return a null value`() {
        testDomainIndexesInURL(
            url = "www.www",
            expectedIndexes = null,
        )
    }

    private fun testDomainIndexesInURL(
        url: String,
        expectedIndexes: Pair<Int, Int>?,
    ) = runTest {
        val urlWithMarkedDomain = url.applyRegistrableDomainSpan(
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined, this),
        )

        assertEquals(expectedIndexes, urlWithMarkedDomain.getRegistrableDomainIndexRange())
    }
}

/**
 * Robolectric default implementation of [InetAddresses] returns false for any address.
 * This shadow is used to override that behavior and return true for any IP address.
 */
@Implements(InetAddresses::class)
private class ShadowInetAddresses {
    companion object {
        @Implementation
        @JvmStatic
        @Suppress("DEPRECATION")
        fun isNumericAddress(address: String): Boolean {
            return Patterns.IP_ADDRESS.matcher(address).matches() || address.contains(":")
        }
    }
}
