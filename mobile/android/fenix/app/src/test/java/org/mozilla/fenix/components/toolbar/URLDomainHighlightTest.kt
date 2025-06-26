/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.net.InetAddresses
import android.util.Patterns
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.runTest
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.util.URLStringUtils
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.toolbar.URLDomainHighlight.getRegistrableDomainOrHostIndexRange
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements

@RunWith(RobolectricTestRunner::class)
@Config(shadows = [ShadowInetAddresses::class])
class URLDomainHighlightTest {
    @Test
    fun `GIVEN a simple URL WHEN getting the domain indexes THEN get the start and end indexes of the domain in display URL`() = runTest {
        val url = "https://www.mozilla.org/"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined, this),
        )

        assertEquals(0 to 11, domainIndexes)
    }

    @Test
    fun `GIVEN a URL with a trailing period in the domain WHEN getting the domain indexes THEN get the start and end indexes of the domain in display URL without the period`() = runTest {
        val url = "https://www.mozilla.org./"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(0 to 11, domainIndexes)
    }

    @Test
    fun `GIVEN a URL with a repeated domain WHEN getting the domain indexes THEN get the start and end indexes of the last domain string occurrence in display URL`() = runTest {
        val url = "https://mozilla.org.mozilla.org/"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(12 to 23, domainIndexes)
    }

    @Test
    fun `GIVEN a URL with a long subdomain and page WHEN getting the domain indexes THEN get the start and end indexes of the domain in display URL`() = runTest {
        val url = "https://firefox-is-an-awesome-browser.mozilla.org/based_on_a_recent_british_study"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(30 to 41, domainIndexes)
    }

    @Test
    fun `GIVEN a URL with an IPv4 address WHEN getting the domain indexes THEN get the start and end indexes of the IP string in display URL`() = runTest {
        val url = "http://127.0.0.1/"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(0 to 9, domainIndexes)
    }

    @Test
    fun `GIVEN a URL with an IPv6 address WHEN getting the domain indexes THEN get the start and end indexes of the IP string in display URL`() = runTest {
        val url = "http://[::1]/"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(0 to 5, domainIndexes)
    }

    @Test
    fun `GIVEN a URL with a non PSL domain WHEN getting the domain indexes THEN get the start and end indexes of the domain in display URL`() = runTest {
        val url = "http://localhost/"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(0 to 9, domainIndexes)
    }

    @Test
    fun `GIVEN an internal page name WHEN getting the domain indexes THEN return a null value`() = runTest {
        val url = "about:mozilla"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertNull(domainIndexes)
    }

    @Test
    fun `GIVEN a content URI WHEN getting the domain indexes THEN return a null value`() = runTest {
        val url = "content://media/external/file/1000000000"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertNull(domainIndexes)
    }

    @Test
    fun `GIVEN a blob URI WHEN getting the domain indexes THEN get the start and end indexes of the domain in display URL`() = runTest {
        val url = "blob:https://www.mozilla.org/69a29afb-938c-4b9e-9fca-b2f79755047a"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(17 to 28, domainIndexes)
    }

    @Test
    fun `GIVEN a blob URI with duplicated blob prefix WHEN getting the domain indexes THEN get the start and end indexes of the domain in display URL`() = runTest {
        val url = "blob:blob:https://www.mozilla.org/69a29afb-938c-4b9e-9fca-b2f79755047a"
        val domainIndexes = getRegistrableDomainOrHostIndexRange(
            url = url,
            displayUrl = URLStringUtils.toDisplayUrl(url).toString(),
            publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
        )

        assertEquals(22 to 33, domainIndexes)
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
