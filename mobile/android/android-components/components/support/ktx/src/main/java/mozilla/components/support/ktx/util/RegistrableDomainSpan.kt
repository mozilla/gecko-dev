/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.util

import android.text.SpannableString
import androidx.core.net.toUri
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.kotlin.isIpv4OrIpv6

private const val BLOB_URL_PREFIX = "blob:"

/**
 * Custom span to mark a range of text as representing the registrable domain part of a URL.
 */
class RegistrableDomainSpan

/**
 * Determines the start and end indexes of either the registrable domain or the full host
 * within a URL string and mark these with [RegistrableDomainSpan].
 *
 * @param cleanURL The complete URL to analyze.
 * @param publicSuffixList The [PublicSuffixList] used to identify the URL host.
 *
 * @return The same [cleanURL] received but with the registrable domain marked with [RegistrableDomainSpan]
 * if this could be identified in the given [cleanURL].
 */
@Suppress("ReturnCount")
internal suspend fun applyRegistrableDomainSpan(
    url: String,
    publicSuffixList: PublicSuffixList,
): CharSequence {
    val spannable = SpannableString(url)

    var offset = 0
    var cleanURL = url
    if (cleanURL.startsWith(BLOB_URL_PREFIX)) {
        cleanURL = cleanURL.removePrefix(BLOB_URL_PREFIX)
        offset += BLOB_URL_PREFIX.length
    }

    val uri = cleanURL.toUri()
    if (!uri.isHttpOrHttps) return spannable

    val host = uri.host ?: return spannable

    val hostStart = cleanURL.indexOf(host)
    if (hostStart == -1) return spannable

    val domainIndexRange = host.getRegistrableDomainIndexRangeInHost(publicSuffixList)
    return spannable.apply {
        when (domainIndexRange) {
            null -> setSpan(
                RegistrableDomainSpan(),
                hostStart + offset,
                (hostStart + host.length + offset).coerceAtMost(length),
                SpannableString.SPAN_EXCLUSIVE_INCLUSIVE,
            )
            else -> setSpan(
                RegistrableDomainSpan(),
                hostStart + domainIndexRange.first + offset,
                (hostStart + domainIndexRange.second + offset).coerceAtMost(length),
                SpannableString.SPAN_EXCLUSIVE_INCLUSIVE,
            )
        }
    }
}

/**
 * Determines the start and end indexes of the registrable domain within the given host string.
 *
 * @param publicSuffixList The [PublicSuffixList] used to identify the URL host.
 *
 * @return A Pair of (startIndex, endIndex) for the registrable domain within the host,
 * or null if the host is an IP address or no registrable domain could be found.
 */
private suspend fun String.getRegistrableDomainIndexRangeInHost(
    publicSuffixList: PublicSuffixList,
): Pair<Int, Int>? {
    if (isIpv4OrIpv6()) return null

    val normalizedHost = removeSuffix(".")

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
