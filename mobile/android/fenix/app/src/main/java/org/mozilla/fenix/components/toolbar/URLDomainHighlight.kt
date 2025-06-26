/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.core.net.toUri
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.kotlin.isIpv4OrIpv6

private const val BLOB_URL_PREFIX = "blob:"

/**
 * Methods used to identify the domain of an URL for highlighting in the UI.
 */
object URLDomainHighlight {
    /**
     * Determines the start and end indexes of either the registrable domain or the full host
     * within a URL string.
     *
     * @param url The complete URL to analyze.
     * @param displayUrl A possible shorter form of [url] suitable for display.
     * @param publicSuffixList The [PublicSuffixList] used to identify the URL host.
     *
     * @return A Pair of (startIndex, endIndex) for either:
     * - The registrable domain's position within the URL, or
     * - The host's position within the URL if no registrable domain was found, or
     * - null if the URL has no host or the host couldn't be located in the URL.
     */
    @Suppress("ReturnCount")
    suspend fun getRegistrableDomainOrHostIndexRange(
        url: String,
        displayUrl: String,
        publicSuffixList: PublicSuffixList,
    ): Pair<Int, Int>? {
        if (url.startsWith(BLOB_URL_PREFIX)) {
            val innerUrl = url.substring(BLOB_URL_PREFIX.length)
            return getRegistrableDomainOrHostIndexRange(
                innerUrl,
                displayUrl,
                publicSuffixList,
            )?.let { (start, end) ->
                BLOB_URL_PREFIX.length + start to BLOB_URL_PREFIX.length + end
            }
        }

        val uri = url.toUri()
        if (!uri.isHttpOrHttps) return null

        val host = uri.host ?: return null

        val hostStart = url.indexOf(host)
        if (hostStart == -1) return null

        val displayUrlOffset = url.indexOf(displayUrl).coerceAtLeast(0)
        val displayedHostStart = hostStart - displayUrlOffset

        val domainIndexRange = getRegistrableDomainIndexRangeInHost(
            host,
            publicSuffixList,
        )
        return domainIndexRange?.let { (start, end) ->
            displayedHostStart + start to displayedHostStart + end
        } ?: (displayedHostStart to displayedHostStart + host.length)
    }

    /**
     * Determines the start and end indexes of the registrable domain within a host string.
     *
     * @param host The host string to analyze
     * @param publicSuffixList The [PublicSuffixList] used to identify the URL host.
     *
     * @return A Pair of (startIndex, endIndex) for the registrable domain within the host,
     * or null if the host is an IP address or no registrable domain could be found.
     */
    private suspend fun getRegistrableDomainIndexRangeInHost(
        host: String,
        publicSuffixList: PublicSuffixList,
    ): Pair<Int, Int>? {
        if (host.isIpv4OrIpv6()) return null

        val normalizedHost = host.removeSuffix(".")

        val registrableDomain = publicSuffixList
            .getPublicSuffixPlusOne(normalizedHost)
            .await() ?: return null

        val start = normalizedHost.lastIndexOf(registrableDomain)
        return if (start == -1) {
            null
        } else {
            start to start + registrableDomain.length
        }
    }
}
