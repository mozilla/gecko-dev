/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.appservices.suggest.Suggestion
import mozilla.appservices.suggest.SuggestionProvider
import mozilla.appservices.suggest.SuggestionQuery
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class FxSuggestSuggestionProviderTest {
    private lateinit var storage: FxSuggestStorage

    @Before
    fun setUp() {
        storage = mock()
        val suggestionProviderConfig = AwesomebarSuggestionProvider(
            availableSuggestionTypes = mapOf(
                SuggestionType.AMP to true,
                SuggestionType.AMP_MOBILE to false,
                SuggestionType.WIKIPEDIA to true,
            ),
        )
        FxSuggestNimbus.features.awesomebarSuggestionProvider.withCachedValue(suggestionProviderConfig)
        GlobalFxSuggestDependencyProvider.storage = storage
    }

    @After
    fun tearDown() {
        FxSuggestNimbus.features.awesomebarSuggestionProvider.withCachedValue(null)
        GlobalFxSuggestDependencyProvider.storage = null
    }

    @Test
    fun inputEmpty() = runTest {
        whenever(storage.query(any())).thenReturn(
            listOf(
                Suggestion.Wikipedia(
                    title = "Las Vegas",
                    url = "https://wikipedia.org/wiki/Las_Vegas",
                    icon = null,
                    iconMimetype = null,
                    fullKeyword = "las",
                ),
            ),
        )

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = true,
            includeSponsoredSuggestions = true,
        )

        val suggestions = provider.onInputChanged("")

        verify(storage, never()).query(any())
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun inputNotEmpty() = runTest {
        whenever(storage.query(any())).thenReturn(
            listOf(
                Suggestion.Amp(
                    title = "Lasagna Come Out Tomorrow",
                    url = "https://www.lasagna.restaurant",
                    rawUrl = "https://www.lasagna.restaurant",
                    icon = listOf(
                        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
                        0, 0, 0, 1, 0, 0, 0, 1, 1, 3, 0, 0, 0, 37, 219, 86, 202, 0,
                        0, 0, 3, 80, 76, 84, 69, 0, 0, 0, 167, 122, 61, 218, 0, 0, 0,
                        1, 116, 82, 78, 83, 0, 64, 230, 216, 102, 0, 0, 0, 10, 73, 68,
                        65, 84, 8, 215, 99, 96, 0, 0, 0, 2, 0, 1, 226, 33, 188, 51, 0,
                        0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
                    ).map(Int::toByte).toByteArray(),
                    iconMimetype = null,
                    fullKeyword = "lasagna",
                    blockId = 0,
                    advertiser = "Good Place Eats",
                    iabCategory = "8 - Food & Drink",
                    impressionUrl = "https://example.com/impression_url",
                    clickUrl = "https://example.com/click_url",
                    rawClickUrl = "https://example.com/click_url",
                    score = 0.3,
                ),
            ),
        )

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = true,
            includeSponsoredSuggestions = true,
        )

        val suggestions = provider.onInputChanged("la")

        verify(storage).query(
            eq(
                SuggestionQuery(
                    keyword = "la",
                    providers = listOf(SuggestionProvider.AMP, SuggestionProvider.WIKIPEDIA),
                    limit = 1,
                ),
            ),
        )
        assertEquals(1, suggestions.size)
        assertEquals("Lasagna Come Out Tomorrow", suggestions[0].title)
        assertEquals(testContext.resources.getString(R.string.sponsored_suggestion_description), suggestions[0].description)
        assertNotNull(suggestions[0].icon)
        assertEquals(Int.MIN_VALUE, suggestions[0].score)
        assertTrue(suggestions[0].metadata.isNullOrEmpty())
    }

    @Test
    fun inputCancelled() = runTest {
        doNothing().`when`(storage).cancelReads()

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = true,
            includeSponsoredSuggestions = true,
        )

        provider.onInputCancelled()

        verify(storage).cancelReads()
    }

    @Test
    fun includeNonSponsoredSuggestionsOnly() = runTest {
        whenever(storage.query(any())).thenReturn(
            listOf(
                Suggestion.Wikipedia(
                    title = "Las Vegas",
                    url = "https://wikipedia.org/wiki/Las_Vegas",
                    icon = null,
                    iconMimetype = null,
                    fullKeyword = "las",
                ),
            ),
        )

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = true,
            includeSponsoredSuggestions = false,
            contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
        )

        val suggestions = provider.onInputChanged("la")

        verify(storage).query(
            eq(
                SuggestionQuery(
                    keyword = "la",
                    providers = listOf(SuggestionProvider.WIKIPEDIA),
                    limit = 1,
                ),
            ),
        )
        assertEquals(1, suggestions.size)
        assertEquals("Las Vegas", suggestions.first().title)
        assertNull(suggestions.first().description)
        assertNull(suggestions.first().icon)
        assertEquals(Int.MIN_VALUE, suggestions.first().score)
        suggestions.first().metadata?.let {
            assertEquals(setOf(FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO, FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO), it.keys)

            val clickInfo = requireNotNull(it[FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO] as? FxSuggestInteractionInfo.Wikipedia)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", clickInfo.contextId)

            val impressionInfo = requireNotNull(it[FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO] as? FxSuggestInteractionInfo.Wikipedia)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)
        }
    }

    @Test
    fun includeSponsoredSuggestionsOnly() = runTest {
        whenever(storage.query(any())).thenReturn(
            listOf(
                Suggestion.Amp(
                    title = "Lasagna Come Out Tomorrow",
                    url = "https://www.lasagna.restaurant",
                    rawUrl = "https://www.lasagna.restaurant",
                    icon = null,
                    iconMimetype = null,
                    fullKeyword = "lasagna",
                    blockId = 0,
                    advertiser = "Good Place Eats",
                    iabCategory = "8 - Food & Drink",
                    impressionUrl = "https://example.com/impression_url",
                    clickUrl = "https://example.com/click_url",
                    rawClickUrl = "https://example.com/click_url",
                    score = 0.3,
                ),
            ),
        )

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = false,
            includeSponsoredSuggestions = true,
            contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
        )

        val suggestions = provider.onInputChanged("la")

        verify(storage).query(
            eq(
                SuggestionQuery(
                    keyword = "la",
                    providers = listOf(SuggestionProvider.AMP),
                    limit = 1,
                ),
            ),
        )
        assertEquals(1, suggestions.size)
        assertEquals("Lasagna Come Out Tomorrow", suggestions.first().title)
        assertEquals(testContext.resources.getString(R.string.sponsored_suggestion_description), suggestions.first().description)
        assertEquals(Int.MIN_VALUE, suggestions.first().score)
        suggestions.first().metadata?.let {
            assertEquals(setOf(FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO, FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO), it.keys)

            val clickInfo = requireNotNull(it[FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO] as? FxSuggestInteractionInfo.Amp)
            assertEquals(0, clickInfo.blockId)
            assertEquals("good place eats", clickInfo.advertiser)
            assertEquals("https://example.com/click_url", clickInfo.reportingUrl)
            assertEquals("8 - Food & Drink", clickInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", clickInfo.contextId)

            val impressionInfo = requireNotNull(it[FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO] as? FxSuggestInteractionInfo.Amp)
            assertEquals(0, impressionInfo.blockId)
            assertEquals("good place eats", impressionInfo.advertiser)
            assertEquals("https://example.com/impression_url", impressionInfo.reportingUrl)
            assertEquals("8 - Food & Drink", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)
        }
    }

    @Test
    fun includeMobileSponsoredSuggestionsOnly() = runTest {
        FxSuggestNimbus.features.awesomebarSuggestionProvider.withCachedValue(
            AwesomebarSuggestionProvider(
                availableSuggestionTypes = mapOf(
                    SuggestionType.AMP to false,
                    SuggestionType.AMP_MOBILE to true,
                    SuggestionType.WIKIPEDIA to true,
                ),
            ),
        )
        whenever(storage.query(any())).thenReturn(
            listOf(
                Suggestion.Amp(
                    title = "Mobile - Lasagna Come Out Tomorrow",
                    url = "https://www.lasagna.restaurant",
                    rawUrl = "https://www.lasagna.restaurant",
                    icon = null,
                    iconMimetype = null,
                    fullKeyword = "lasagna",
                    blockId = 0,
                    advertiser = "Good Place Eats",
                    iabCategory = "8 - Food & Drink",
                    impressionUrl = "https://example.com/impression_url",
                    clickUrl = "https://example.com/click_url",
                    rawClickUrl = "https://example.com/click_url",
                    score = 0.3,
                ),
            ),
        )

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = false,
            includeSponsoredSuggestions = true,
            contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
        )

        val suggestions = provider.onInputChanged("la")

        verify(storage).query(
            eq(
                SuggestionQuery(
                    keyword = "la",
                    providers = listOf(SuggestionProvider.AMP_MOBILE),
                    limit = 1,
                ),
            ),
        )
        assertEquals(1, suggestions.size)
        assertEquals("Mobile - Lasagna Come Out Tomorrow", suggestions.first().title)
        assertEquals(testContext.resources.getString(R.string.sponsored_suggestion_description), suggestions.first().description)
        assertEquals(Int.MIN_VALUE, suggestions.first().score)
        suggestions.first().metadata?.let {
            assertEquals(setOf(FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO, FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO), it.keys)

            val clickInfo = requireNotNull(it[FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO] as? FxSuggestInteractionInfo.Amp)
            assertEquals(0, clickInfo.blockId)
            assertEquals("good place eats", clickInfo.advertiser)
            assertEquals("https://example.com/click_url", clickInfo.reportingUrl)
            assertEquals("8 - Food & Drink", clickInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", clickInfo.contextId)

            val impressionInfo = requireNotNull(it[FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO] as? FxSuggestInteractionInfo.Amp)
            assertEquals(0, impressionInfo.blockId)
            assertEquals("good place eats", impressionInfo.advertiser)
            assertEquals("https://example.com/impression_url", impressionInfo.reportingUrl)
            assertEquals("8 - Food & Drink", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)
        }
    }

    @Test
    fun includeSponsoredSuggestionsOnlyWhenAmpUnavailable() = runTest {
        FxSuggestNimbus.features.awesomebarSuggestionProvider.withCachedValue(
            AwesomebarSuggestionProvider(availableSuggestionTypes = emptyMap()),
        )
        whenever(storage.query(any())).thenReturn(emptyList())

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = false,
            includeSponsoredSuggestions = true,
            contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
        )

        val suggestions = provider.onInputChanged("la")

        verify(storage).query(
            eq(
                SuggestionQuery(
                    keyword = "la",
                    providers = emptyList(),
                    limit = 1,
                ),
            ),
        )
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun includeNonSponsoredSuggestionsOnlyWhenWikipediaUnavailable() = runTest {
        FxSuggestNimbus.features.awesomebarSuggestionProvider.withCachedValue(
            AwesomebarSuggestionProvider(availableSuggestionTypes = emptyMap()),
        )
        whenever(storage.query(any())).thenReturn(emptyList())

        val provider = FxSuggestSuggestionProvider(
            resources = testContext.resources,
            loadUrlUseCase = mock(),
            includeNonSponsoredSuggestions = true,
            includeSponsoredSuggestions = false,
            contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
        )

        val suggestions = provider.onInputChanged("la")

        verify(storage).query(
            eq(
                SuggestionQuery(
                    keyword = "la",
                    providers = emptyList(),
                    limit = 1,
                ),
            ),
        )
        assertTrue(suggestions.isEmpty())
    }
}
