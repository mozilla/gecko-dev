/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.trendingsearches

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
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

    @Test
    fun `GIVEN a search engine WHEN getting trending searches THEN the corresponding response is returned`() = runTest {
        val client = TrendingSearchClient(GOOGLE_MOCK_RESPONSE)
        client.setSearchEngine(searchEngine)
        val expectedResults = listOf("firefox", "firefox for mac", "firefox quantum", "firefox update", "firefox esr", "firefox focus", "firefox addons", "firefox extensions", "firefox nightly", "firefox clear cache")

        val results = client.getTrendingSearches()

        assertEquals(expectedResults, results)
    }

    @Test(expected = TrendingSearchClient.ResponseParserException::class)
    fun `GIVEN a bad server response WHEN getting trending searches THEN throw a parser exception`() = runTest {
        val client = TrendingSearchClient(SERVER_ERROR_RESPONSE)
        client.setSearchEngine(searchEngine)

        client.getTrendingSearches()
    }

    @Test(expected = TrendingSearchClient.FetchException::class)
    fun `GIVEN an exception in the trending search fetcher WHEN getting trending searches THEN re-throw an IOException`() = runTest {
        val client = TrendingSearchClient { throw IOException() }
        client.setSearchEngine(searchEngine)

        client.getTrendingSearches()
    }

    @Test
    fun `GIVEN a search engine without a trending URL WHEN getting trending searches THEN return an empty suggestion list`() = runTest {
        val testSearchEngine = createSearchEngine(
            name = "Test",
            url = "https://localhost?q={searchTerms}",
            icon = mock(),
        )

        val client = TrendingSearchClient { "no-op" }
        client.setSearchEngine(testSearchEngine)

        val results = client.getTrendingSearches()

        assertEquals(emptyList<String>(), results)
    }
}
