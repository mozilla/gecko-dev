/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.trendingsearches

import mozilla.components.browser.state.search.SearchEngine
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
    private val fetcher: TrendingSearchFetcher,
) {
    private val logger = Logger("TrendingSearchClient")
    private var searchEngine: SearchEngine? = null

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
        searchEngine?.let {
            if (!it.canProvideTrendingSearches) {
                // This search engine doesn't support trending searches.
                return emptyList()
            }

            val trendingURL = it.buildTrendingURL()

            val parser = selectResponseParser(it)

            val trendingSearchResults = try {
                trendingURL?.let { url -> fetcher(url) }
            } catch (_: IOException) {
                throw FetchException()
            }

            return try {
                trendingSearchResults?.let(parser)
            } catch (_: JSONException) {
                throw ResponseParserException()
            }
        }

        logger.warn("No default search engine for fetching trending searches")
        return emptyList()
    }

    /**
     * Sets the search engine used to fetch trending suggestions.
     */
    fun setSearchEngine(searchEngine: SearchEngine?) {
        this.searchEngine = searchEngine
    }

    /**
     * Returns the search engine used to fetch trending suggestions.
     */
    fun getSearchEngine(): SearchEngine? {
        return this.searchEngine
    }
}
