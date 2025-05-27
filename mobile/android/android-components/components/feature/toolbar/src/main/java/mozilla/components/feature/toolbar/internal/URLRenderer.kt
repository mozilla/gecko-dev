/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar.internal

import android.text.SpannableStringBuilder
import android.text.SpannableStringBuilder.SPAN_INCLUSIVE_INCLUSIVE
import android.text.style.ForegroundColorSpan
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.trySendBlocking
import kotlinx.coroutines.launch
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.kotlin.isIpv4OrIpv6

/**
 * Asynchronous URL renderer.
 *
 * This "renderer" will create a (potentially) colored URL (using spans) in a coroutine and set it on the [Toolbar].
 */
internal class URLRenderer(
    private val toolbar: Toolbar,
    private val configuration: ToolbarFeature.UrlRenderConfiguration?,
) {
    private val scope = CoroutineScope(Dispatchers.Main)

    @VisibleForTesting internal var job: Job? = null

    @VisibleForTesting internal val channel = Channel<String>(capacity = Channel.CONFLATED)

    /**
     * Starts this renderer which will listen for incoming URLs to render.
     */
    fun start() {
        job = scope.launch {
            for (url in channel) {
                updateUrl(url)
            }
        }
    }

    /**
     * Stops this renderer.
     */
    fun stop() {
        job?.cancel()
    }

    /**
     * Posts this [url] to the renderer.
     */
    fun post(url: String) {
        try {
            channel.trySendBlocking(url)
        } catch (e: InterruptedException) {
            // Ignore
        }
    }

    @VisibleForTesting
    internal suspend fun updateUrl(url: String) {
        if (url.isEmpty() || configuration == null) {
            toolbar.url = url
            return
        }

        toolbar.url = when (configuration.renderStyle) {
            // Display only the eTLD+1 (direct subdomain of the public suffix), uncolored
            ToolbarFeature.RenderStyle.RegistrableDomain -> {
                val host = url.toUri().host?.ifEmpty { null }
                host?.let { getRegistrableDomain(host, configuration) } ?: url
            }
            // Display the registrableDomain with color and URL with another color
            ToolbarFeature.RenderStyle.ColoredUrl -> SpannableStringBuilder(url).apply {
                val span = getRegistrableDomainOrHostSpan(url, configuration.publicSuffixList)

                if (configuration.urlColor != null && span != null) {
                    applyUrlColors(
                        configuration.urlColor,
                        configuration.registrableDomainColor,
                        span,
                    )
                }
            }
            // Display the full URL, uncolored
            ToolbarFeature.RenderStyle.UncoloredUrl -> url
        }
    }
}

private suspend fun getRegistrableDomain(host: String, configuration: ToolbarFeature.UrlRenderConfiguration) =
    configuration.publicSuffixList.getPublicSuffixPlusOne(host).await()

/**
 * Determines the position span of the registrable domain within a host string.
 *
 * @param host The host string to analyze
 * @param publicSuffixList The [PublicSuffixList] used to get the eTLD+1 for the host
 * @return A Pair of (startIndex, endIndex) for the registrable domain within the host,
 *         or null if the host is an IP address or no registrable domain could be found
 */
@VisibleForTesting
internal suspend fun getRegistrableDomainSpanInHost(
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

/**
 * Determines the position span of either the registrable domain or the full host
 * within a URL string.
 *
 * @param url The complete URL to analyze
 * @param publicSuffixList The [PublicSuffixList] used to get the eTLD+1 for the host
 * @return A Pair of (startIndex, endIndex) for either:
 *         - The registrable domain's position within the URL, or
 *         - The host's position within the URL if no registrable domain was found, or
 *         - null if the URL has no host or the host couldn't be located in the URL
 */
@Suppress("ReturnCount")
@VisibleForTesting
internal suspend fun getRegistrableDomainOrHostSpan(
    url: String,
    publicSuffixList: PublicSuffixList,
): Pair<Int, Int>? {
    val uri = url.toUri()
    if (!uri.isHttpOrHttps) return null

    val host = uri.host ?: return null

    val hostStart = url.indexOf(host)
    if (hostStart == -1) return null

    val domainSpan = getRegistrableDomainSpanInHost(host, publicSuffixList)
    return domainSpan?.let { (start, end) ->
        hostStart + start to hostStart + end
    } ?: (hostStart to hostStart + host.length)
}

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
        ForegroundColorSpan(registrableDomainColor),
        start,
        end,
        SPAN_INCLUSIVE_INCLUSIVE,
    )
}
