/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.graphics.Bitmap
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.feature.awesomebar.facts.emitTrendingSearchSuggestionClickedFact
import mozilla.components.feature.awesomebar.facts.emitTrendingSearchSuggestionsDisplayedFact
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.ext.buildSearchUrl
import mozilla.components.feature.search.trendingsearches.TrendingSearchClient
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.sanitizeURL
import java.io.IOException
import java.util.UUID
import java.util.concurrent.TimeUnit

// Return 4 trending searches by default.
const val DEFAULT_TRENDING_SEARCHES_LIMIT = 4
const val TRENDING_SEARCHES_MAXIMUM_ALLOWED_SUGGESTIONS_LIMIT: Int = 1000

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides trending search suggestions from
 * the passed in [SearchEngine].
 */
class TrendingSearchProvider private constructor(
    internal val client: TrendingSearchClient,
    private val searchUseCase: SearchUseCases.SearchUseCase,
    private val limit: Int = DEFAULT_TRENDING_SEARCHES_LIMIT,
    internal val engine: Engine? = null,
    private val icon: Bitmap? = null,
    private val suggestionsHeader: String? = null,
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()
    private val logger = Logger("TrendingSearchProvider")

    init {
        require(limit >= 1) { "limit needs to be >= 1" }
    }

    /**
     * Creates a [TrendingSearchProvider] using the default engine as provided by the given
     * [BrowserStore].
     *
     * @param store The [BrowserStore] to look up search engines.
     * @param fetchClient The HTTP client for requesting suggestions from the search engine.
     * @param privateMode When set to `true` then all requests to search engines will be made in private mode.
     * @param searchUseCase The use case to invoke for searches.
     * @param limit The maximum number of suggestions that should be returned. It needs to be >= 1.
     * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
     * highest scored search suggestion URL.
     * @param icon The image to display next to the result. If not specified, the engine icon is used.
     * @param suggestionsHeader Optional suggestions header to display.
     */
    constructor(
        store: BrowserStore,
        fetchClient: Client,
        privateMode: Boolean,
        searchUseCase: SearchUseCases.SearchUseCase,
        limit: Int = DEFAULT_TRENDING_SEARCHES_LIMIT,
        engine: Engine? = null,
        icon: Bitmap? = null,
        suggestionsHeader: String? = null,
    ) : this (
        TrendingSearchClient(store) { url -> fetch(fetchClient, url, privateMode) },
        searchUseCase,
        limit,
        engine,
        icon,
        suggestionsHeader,
    )

    override fun groupTitle(): String? {
        return suggestionsHeader
    }

    @Suppress("ReturnCount")
    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isNotEmpty()) {
            return emptyList()
        }

        val suggestions = fetchTrendingSearches()

        return suggestions.toAwesomebarSuggestions().also {
            // Call speculativeConnect for URL of first (highest scored) suggestion
            it.firstOrNull()?.title?.let { searchTerms -> maybeCallSpeculativeConnect(searchTerms) }
            if (it.isNotEmpty()) {
                emitTrendingSearchSuggestionsDisplayedFact(it.size)
            }
        }
    }

    private fun maybeCallSpeculativeConnect(searchTerms: String) {
        client.searchEngine?.let { searchEngine ->
            engine?.speculativeConnect(searchEngine.buildSearchUrl(searchTerms))
        }
    }

    private suspend fun fetchTrendingSearches(): List<String>? {
        return try {
            client.getTrendingSearches()
        } catch (e: TrendingSearchClient.FetchException) {
            logger.info("Could not fetch search suggestions from search engine", e)
            // If we can't fetch trending searches then just return an empty list
            emptyList()
        } catch (e: TrendingSearchClient.ResponseParserException) {
            logger.warn("Could not parse search suggestions from search engine", e)
            // If parsing failed then just return an empty list
            emptyList()
        }
    }

    private fun List<String>?.toAwesomebarSuggestions(): List<AwesomeBar.Suggestion> = this?.let {
        this.distinct().take(limit).mapIndexed { index, item ->
            AwesomeBar.Suggestion(
                provider = this@TrendingSearchProvider,
                id = item,
                title = item,
                editSuggestion = item,
                icon = icon ?: client.searchEngine?.icon,
                // Reducing MAX_VALUE to allow other providers to go above these suggestions,
                // for which they need additional spots to be available.
                score = Int.MAX_VALUE - (index + TRENDING_SEARCHES_MAXIMUM_ALLOWED_SUGGESTIONS_LIMIT + 2),
                onSuggestionClicked = {
                    searchUseCase.invoke(item)
                    emitTrendingSearchSuggestionClickedFact(index)
                },
            )
        }
    } ?: emptyList()

    /**
     * Companion containing method and constants used to fetch suggestions
     */
    companion object {
        // Timeout to be used when reading from a resource.
        private const val READ_TIMEOUT_IN_MS = 2000L

        // Timeout to be used when connecting to a resource.
        private const val CONNECT_TIMEOUT_IN_MS = 1000L

        /**
         * Uses the fetchClient to make a request to the provided url to fetch suggestions
         *
         * @param fetchClient The [Client] used to make the request.
         * @param url The url of the request.
         * @param privateMode Whether the request should be performed in a private context.
         */
        @Suppress("ReturnCount", "TooGenericExceptionCaught")
        private fun fetch(fetchClient: Client, url: String, privateMode: Boolean): String? {
            try {
                val request = Request(
                    url = url.sanitizeURL(),
                    readTimeout = Pair(READ_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS),
                    connectTimeout = Pair(CONNECT_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS),
                    private = privateMode,
                    cookiePolicy = Request.CookiePolicy.OMIT,
                    useCaches = false,
                )

                val response = fetchClient.fetch(request)
                if (!response.isSuccess) {
                    response.close()
                    return null
                }

                return response.use { it.body.string() }
            } catch (e: IOException) {
                return null
            } catch (e: ArrayIndexOutOfBoundsException) {
                // On some devices we are seeing an ArrayIndexOutOfBoundsException being thrown
                // somewhere inside AOSP/okhttp.
                // See: https://github.com/mozilla-mobile/android-components/issues/964
                return null
            }
        }
    }
}
