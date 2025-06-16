/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import androidx.core.net.toUri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import org.mozilla.fenix.trackingprotection.CookieBannerUIMode
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/**
 * Get the current status of cookie banner ui mode.
 *
 * @param tab The tab for which to get the cookie banner ui mode.
 * @param isFeatureEnabledInPrivateMode Whether the cookie banner feature is enabled in private mode.
 * @param publicSuffixList [PublicSuffixList] used to obtain the base domain of the current site.
 */
suspend fun CookieBannersStorage.getCookieBannerUIMode(
    tab: SessionState,
    isFeatureEnabledInPrivateMode: Boolean,
    publicSuffixList: PublicSuffixList,
): CookieBannerUIMode {
    return if (isFeatureEnabledInPrivateMode) {
        val isSiteDomainReported = withContext(Dispatchers.IO) {
            val host = tab.content.url.toUri().host.orEmpty()
            val siteDomain = publicSuffixList.getPublicSuffixPlusOne(host).await()
            siteDomain?.let { isSiteDomainReported(it) }
        }

        if (isSiteDomainReported == true) {
            return CookieBannerUIMode.REQUEST_UNSUPPORTED_SITE_SUBMITTED
        }

        val hasException = withContext(Dispatchers.IO) {
            hasException(tab.content.url, tab.content.private)
        } ?: return CookieBannerUIMode.HIDE

        if (hasException) {
            CookieBannerUIMode.DISABLE
        } else {
            withContext(Dispatchers.Main) {
                tab.isCookieBannerSupported()
            }
        }
    } else {
        CookieBannerUIMode.HIDE
    }
}

private suspend fun SessionState.isCookieBannerSupported(): CookieBannerUIMode =
    suspendCoroutine { continuation ->
        engineState.engineSession?.hasCookieBannerRuleForSession(
            onResult = { isSupported ->
                val mode = if (isSupported) {
                    CookieBannerUIMode.ENABLE
                } else {
                    CookieBannerUIMode.SITE_NOT_SUPPORTED
                }
                continuation.resume(mode)
            },
            onException = {
                continuation.resume(CookieBannerUIMode.HIDE)
            },
        )
    }
