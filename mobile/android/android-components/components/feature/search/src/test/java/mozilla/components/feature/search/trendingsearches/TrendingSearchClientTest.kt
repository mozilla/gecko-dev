/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.trendingsearches

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class TrendingSearchClientTest {
    companion object {
        val GOOGLE_MOCK_RESPONSE: TrendingSearchFetcher = { "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]" }
        val SERVER_ERROR_RESPONSE: TrendingSearchFetcher = { "Server error. Try again later" }
    }

    private val searchEngine = createSearchEngine(
        name = "Test",
        url = "https://localhost?q={searchTerms}",
        trendingUrl = "https://localhost/suggestions?q={searchTerms}",
        icon = mock(),
    )

    private val store = BrowserStore(
        initialState = BrowserState(
            search = SearchState(
                regionSearchEngines = listOf(searchEngine),
            ),
        ),
    )

    @Test
    fun `GIVEN a search engine WHEN getting trending searches THEN the corresponding response is returned`() = runTest {
        val client = TrendingSearchClient(store, GOOGLE_MOCK_RESPONSE)
        val expectedResults = listOf("firefox", "firefox for mac", "firefox quantum", "firefox update", "firefox esr", "firefox focus", "firefox addons", "firefox extensions", "firefox nightly", "firefox clear cache")

        val results = client.getTrendingSearches()

        assertEquals(expectedResults, results)
    }

    @Test(expected = TrendingSearchClient.ResponseParserException::class)
    fun `GIVEN a bad server response WHEN getting trending searches THEN throw a parser exception`() = runTest {
        val client = TrendingSearchClient(store, SERVER_ERROR_RESPONSE)

        client.getTrendingSearches()
    }

    @Test(expected = TrendingSearchClient.FetchException::class)
    fun `GIVEN an exception in the trending search fetcher WHEN getting trending searches THEN re-throw an IOException`() = runTest {
        val client = TrendingSearchClient(store) { throw IOException() }

        client.getTrendingSearches()
    }

    @Test
    fun `GIVEN a search engine without a trending URL WHEN getting trending searches THEN return an empty suggestion list`() = runTest {
        val testSearchEngine = createSearchEngine(
            name = "Test",
            url = "https://localhost?q={searchTerms}",
            icon = mock(),
        )

        val testStore = BrowserStore(
            initialState = BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(testSearchEngine),
                ),
            ),
        )

        val client = TrendingSearchClient(testStore) { "no-op" }

        val results = client.getTrendingSearches()

        assertEquals(emptyList<String>(), results)
    }
}
