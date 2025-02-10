/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.trendingsearches

import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.ext.buildTrendingURL
import mozilla.components.feature.search.ext.canProvideTrendingSearches
import mozilla.components.feature.search.suggestions.selectResponseParser
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import java.io.IOException

/**
 * Async function responsible for taking a URL and returning the results
 */
typealias TrendingSearchFetcher = suspend (url: String) -> String?

/**
 *  Provides an interface to get trending searches from a given SearchEngine.
 */
class TrendingSearchClient(
    store: BrowserStore,
    private val fetcher: TrendingSearchFetcher,
) {
    private val logger = Logger("TrendingSearchClient")
    val searchEngine = store.state.search.selectedOrDefaultSearchEngine

    /**
     * Exception types for errors caught while getting a list of trending searches.
     */
    class FetchException : Exception("There was a problem fetching trending searches")

    /**
     * Exception types for errors caught while parsing the trending searches response.
     */
    class ResponseParserException : Exception("There was a problem parsing the trending searches response")

    /**
     * Returns a list of trending searches from the search engine's trending search URL endpoint.
     */
    suspend fun getTrendingSearches(): List<String>? {
        if (searchEngine == null) {
            logger.warn("No default search engine for fetching trending searches")
            return emptyList()
        }

        if (!searchEngine.canProvideTrendingSearches) {
            // This search engine doesn't support trending searches.
            return emptyList()
        }

        val trendingURL = searchEngine.buildTrendingURL()

        val parser = selectResponseParser(searchEngine)

        val trendingSearchResults = try {
            trendingURL?.let { fetcher(it) }
        } catch (_: IOException) {
            throw FetchException()
        }

        return try {
            trendingSearchResults?.let(parser)
        } catch (_: JSONException) {
            throw ResponseParserException()
        }
    }
}
