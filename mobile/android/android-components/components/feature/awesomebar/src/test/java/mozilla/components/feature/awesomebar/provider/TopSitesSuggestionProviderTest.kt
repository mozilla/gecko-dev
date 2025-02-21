/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.feature.top.sites.DefaultTopSitesStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class TopSitesSuggestionProviderTest {
    // Contains 4 top sites, one for every top site type
    private val topSites = listOf(
        TopSite.Frecent(
            id = 0,
            title = "mozilla frecent",
            url = "http://www.mozilla.com/frecent",
            createdAt = 0,
        ),
        TopSite.Pinned(
            id = 1,
            title = "mozilla pinned",
            url = "http://www.mozilla.com/pinned",
            createdAt = 0,
        ),
        TopSite.Default(
            id = 2,
            title = "mozilla default",
            url = "http://www.mozilla.com/default",
            createdAt = 0,
        ),
        TopSite.Provided(
            id = 3,
            title = "mozilla provided",
            url = "http://www.mozilla.com/provided",
            clickUrl = "",
            imageUrl = "",
            impressionUrl = "",
            createdAt = 0,
        ),
    )

    @Test
    fun `GIVEN text is not empty WHEN input is changed THEN provider returns an empty list`() = runTest {
        val provider = TopSitesSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("fire")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `GIVEN text is empty WHEN input is changed THEN provider returns suggestions for only pinned and frecent sites from top sites storage by default`() = runTest {
        val storage: DefaultTopSitesStorage = mock()

        whenever(storage.cachedTopSites).thenReturn(topSites)

        val provider = TopSitesSuggestionProvider(storage, mock())
        val suggestions = provider.onInputChanged("")
        assertEquals(2, suggestions.size)
        assertEquals(topSites[0].title, suggestions[0].title)
        assertEquals(topSites[1].title, suggestions[1].title)
    }

    @Test
    fun `GIVEN custom top sites filter WHEN input is changed THEN provider returns suggestions from top sites storage using the filter`() = runTest {
        val storage: DefaultTopSitesStorage = mock()

        whenever(storage.cachedTopSites).thenReturn(topSites)

        val provider = TopSitesSuggestionProvider(
            topSitesStorage = storage,
            loadUrlUseCase = mock(),
            topSitesFilter = { it.filterIsInstance<TopSite.Default>() },
        )

        val suggestions = provider.onInputChanged("")
        assertEquals(1, suggestions.size)
        assertEquals(topSites[2].title, suggestions[0].title)
    }

    @Test
    fun `GIVEN limit that is less than total number of results WHEN input is changed THEN provider returns suggestions within the limit`() = runTest {
        val storage: DefaultTopSitesStorage = mock()

        whenever(storage.cachedTopSites).thenReturn(topSites)

        val provider = TopSitesSuggestionProvider(
            topSitesStorage = storage,
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 1,
        )

        val suggestions = provider.onInputChanged("")
        assertEquals(1, suggestions.size)
    }

    @Test
    fun `GIVEN top sites cache is empty WHEN input is changed THEN provider returns an empty list`() = runTest {
        val storage: DefaultTopSitesStorage = mock()
        doReturn(emptyList<TopFrecentSiteInfo>()).`when`(storage).cachedTopSites
        val provider = TopSitesSuggestionProvider(
            topSitesStorage = storage,
            loadUrlUseCase = mock(),
        )

        val suggestions = provider.onInputChanged("")
        assertEquals(0, suggestions.size)
    }

    @Test
    fun `WHEN input is changed THEN provider calls speculativeConnect for URL of highest scored suggestion`() = runTest {
        val storage: DefaultTopSitesStorage = mock()
        val engine: Engine = mock()
        val provider = TopSitesSuggestionProvider(storage, mock(), engine = engine)

        var suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        whenever(storage.cachedTopSites).thenReturn(topSites)

        suggestions = provider.onInputChanged("")
        assertEquals(2, suggestions.size)
        verify(engine, times(1)).speculativeConnect(topSites[0].url)
    }

    @Test
    fun `GIVEN top site suggestion is clicked THEN suggestion clicked fact is emitted`() {
        runTest {
            val storage: DefaultTopSitesStorage = mock()
            val engine: Engine = mock()
            val provider = TopSitesSuggestionProvider(storage, mock(), engine = engine)

            var suggestions = provider.onInputChanged("")
            assertTrue(suggestions.isEmpty())
            verify(engine, never()).speculativeConnect(anyString())

            whenever(storage.cachedTopSites).thenReturn(topSites)

            suggestions = provider.onInputChanged("")
            assertEquals(2, suggestions.size)
            verify(engine, times(1)).speculativeConnect(topSites[0].url)

            CollectionProcessor.withFactCollection { facts ->
                suggestions[1].onSuggestionClicked!!.invoke()

                assertEquals(1, facts.size)
                facts[0].apply {
                    assertEquals(Component.FEATURE_AWESOMEBAR, component)
                    assertEquals(Action.INTERACTION, action)
                    assertEquals(AwesomeBarFacts.Items.TOP_SITE_SUGGESTION_CLICKED, item)
                }
            }
        }
    }
}
