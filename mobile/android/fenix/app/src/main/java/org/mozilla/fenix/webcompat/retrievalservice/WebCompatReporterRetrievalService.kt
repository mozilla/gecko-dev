/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.retrievalservice

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.webcompat.store.WebCompatInfoDeserializer
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/**
 * Service that handles the submission requests for the report broken site feature.
 */
interface WebCompatReporterRetrievalService {

    /**
     * Returns [WebCompatInfoDto] or null if the services fails to retrieve the data.
     */
    suspend fun retrieveInfo(): WebCompatInfoDto?
}

/**
 * The default implementation of [WebCompatReporterRetrievalService].
 *
 * @param browserStore [BrowserStore] used to access [BrowserState].
 * @param webCompatInfoDeserializer Used to deserialize Json to [WebCompatInfoDto].
 */
class DefaultWebCompatReporterRetrievalService(
    private val browserStore: BrowserStore,
    private val webCompatInfoDeserializer: WebCompatInfoDeserializer,
) : WebCompatReporterRetrievalService {

    private val logger = Logger("WebCompatReporterRetrievalService")

    override suspend fun retrieveInfo(): WebCompatInfoDto? = withContext(Dispatchers.Main) {
        suspendCoroutine { continuation ->
            browserStore.state.selectedTab?.engineState?.engineSession?.getWebCompatInfo(
                onResult = { details ->
                    val webCompatInfo = webCompatInfoDeserializer.decode(string = details.toString())
                    continuation.resume(webCompatInfo)
                },
                onException = {
                    logger.error("Error retrieving web compat info", it)
                    continuation.resume(null)
                },
            )
        }
    }
}
