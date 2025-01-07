/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.di

import kotlinx.coroutines.CoroutineScope
import kotlinx.serialization.json.Json
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.webcompat.WebCompatState
import org.mozilla.fenix.webcompat.retrievalservice.DefaultWebCompatReporterRetrievalService
import org.mozilla.fenix.webcompat.store.WebCompatInfoDeserializer
import org.mozilla.fenix.webcompat.store.WebCompatReporterNavigationMiddleware
import org.mozilla.fenix.webcompat.store.WebCompatReporterStorageMiddleware
import org.mozilla.fenix.webcompat.store.WebCompatReporterSubmissionMiddleware

/**
 * Provides middleware for the WebCompat Reporter store.
 */
object WebCompatReporterMiddlewareProvider {

    /**
     * Provides middleware for the WebCompat Reporter.
     *
     * @param browserStore [BrowserStore] used to access [BrowserState].
     * @param appStore [AppStore] used to persist [WebCompatState].
     * @param scope The [CoroutineScope] used for launching coroutines.
     */
    fun provideMiddleware(
        browserStore: BrowserStore,
        appStore: AppStore,
        scope: CoroutineScope,
    ) = listOf(
        provideStorageMiddleware(appStore),
        provideSubmissionMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            webCompatInfoDeserializer = provideWebCompatInfoDeserializer(),
            scope = scope,
        ),
        provideNavigationMiddleware(),
    )

    private fun provideStorageMiddleware(
        appStore: AppStore,
    ) = WebCompatReporterStorageMiddleware(
        appStore = appStore,
    )

    private fun provideSubmissionMiddleware(
        appStore: AppStore,
        browserStore: BrowserStore,
        webCompatInfoDeserializer: WebCompatInfoDeserializer,
        scope: CoroutineScope,
    ) = WebCompatReporterSubmissionMiddleware(
        appStore = appStore,
        webCompatReporterRetrievalService = DefaultWebCompatReporterRetrievalService(
            browserStore = browserStore,
            webCompatInfoDeserializer = webCompatInfoDeserializer,
        ),
        scope = scope,
    )

    private fun provideNavigationMiddleware() =
        WebCompatReporterNavigationMiddleware()

    private val json by lazy {
        Json {
            ignoreUnknownKeys = true
            useAlternativeNames = false
        }
    }

    internal fun provideWebCompatInfoDeserializer() = WebCompatInfoDeserializer(json = json)
}
