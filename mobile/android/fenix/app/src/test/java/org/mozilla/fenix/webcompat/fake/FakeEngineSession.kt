/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.fake

import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.shopping.ProductAnalysis
import mozilla.components.concept.engine.shopping.ProductAnalysisStatus
import mozilla.components.concept.engine.shopping.ProductRecommendation
import mozilla.components.concept.engine.translate.TranslationOptions
import org.json.JSONObject

/**
 * A fake engine session exposing a [jsonString] property that can be changed at test runtime.
 *
 * @param jsonString A string that is converted into a JSONObject and used when getting WebCompat info.
 */
internal class FakeEngineSession(
    private val jsonString: String,
) : EngineSession() {

    override val settings: Settings
        get() = DefaultSettings()

    override fun getWebCompatInfo(
        onResult: (JSONObject) -> Unit,
        onException: (Throwable) -> Unit,
    ) {
        onResult(JSONObject(jsonString))
    }

    override fun loadUrl(
        url: String,
        parent: EngineSession?,
        flags: LoadUrlFlags,
        additionalHeaders: Map<String, String>?,
    ) {}

    override fun loadData(data: String, mimeType: String, encoding: String) {}

    override fun requestPdfToDownload() {}

    override fun requestPrintContent() {}

    override fun stopLoading() {}

    override fun reload(flags: LoadUrlFlags) {}

    override fun goBack(userInteraction: Boolean) {}

    override fun goForward(userInteraction: Boolean) {}

    override fun goToHistoryIndex(index: Int) {}

    override fun restoreState(state: EngineSessionState): Boolean { return false }

    override fun updateTrackingProtection(policy: TrackingProtectionPolicy) {}

    override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {}

    override fun hasCookieBannerRuleForSession(
        onResult: (Boolean) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun checkForPdfViewer(
        onResult: (Boolean) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun requestProductRecommendations(
        url: String,
        onResult: (List<ProductRecommendation>) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun requestProductAnalysis(
        url: String,
        onResult: (ProductAnalysis) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun reanalyzeProduct(
        url: String,
        onResult: (String) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun requestAnalysisStatus(
        url: String,
        onResult: (ProductAnalysisStatus) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun sendClickAttributionEvent(
        aid: String,
        onResult: (Boolean) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun sendImpressionAttributionEvent(
        aid: String,
        onResult: (Boolean) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun sendPlacementAttributionEvent(
        aid: String,
        onResult: (Boolean) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun reportBackInStock(
        url: String,
        onResult: (String) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun requestTranslate(
        fromLanguage: String,
        toLanguage: String,
        options: TranslationOptions?,
    ) {}

    override fun requestTranslationRestore() {}

    override fun getNeverTranslateSiteSetting(
        onResult: (Boolean) -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun setNeverTranslateSiteSetting(
        setting: Boolean,
        onResult: () -> Unit,
        onException: (Throwable) -> Unit,
    ) {}

    override fun findAll(text: String) {}

    override fun findNext(forward: Boolean) {}

    override fun clearFindMatches() {}

    override fun exitFullScreenMode() {}

    override fun purgeHistory() {}
}
