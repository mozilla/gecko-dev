/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.graphics.Bitmap
import androidx.core.graphics.drawable.toBitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.icons.R
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import java.io.IOException

private const val GOOGLE_MOCK_RESPONSE = "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"
private const val GOOGLE_MOCK_RESPONSE_WITH_DUPLICATES = "[\"firefox\",[\"firefox\",\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class TrendingSearchProviderTest {
    @Test
    fun `GIVEN text is empty WHEN input is changed THEN provider returns suggestions based on search engine`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                trendingUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = TrendingSearchProvider(
                fetchClient = HttpURLConnectionClient(),
                privateMode = false,
                searchUseCase = useCase,
                limit = 11,
            )
            provider.setSearchEngine(searchEngine)

            try {
                val suggestions = provider.onInputChanged("")

                assertEquals(10, suggestions.size)

                assertEquals("firefox", suggestions[0].title)
                assertEquals("firefox for mac", suggestions[1].title)
                assertEquals("firefox quantum", suggestions[2].title)
                assertEquals("firefox update", suggestions[3].title)
                assertEquals("firefox esr", suggestions[4].title)
                assertEquals("firefox focus", suggestions[5].title)
                assertEquals("firefox addons", suggestions[6].title)
                assertEquals("firefox extensions", suggestions[7].title)
                assertEquals("firefox nightly", suggestions[8].title)
                assertEquals("firefox clear cache", suggestions[9].title)

                Mockito.verify(useCase, Mockito.never())
                    .invoke(ArgumentMatchers.anyString(), any(), any())

                // Search suggestions should leave room for other providers' suggestions above
                assertNull(
                    suggestions.firstOrNull {
                        it.score > Int.MAX_VALUE - (SEARCH_TERMS_MAXIMUM_ALLOWED_SUGGESTIONS_LIMIT + 2)
                    },
                )

                CollectionProcessor.withFactCollection { facts ->
                    suggestions[5].onSuggestionClicked!!.invoke()

                    assertEquals(1, facts.size)
                    facts[0].apply {
                        assertEquals(Component.FEATURE_AWESOMEBAR, component)
                        assertEquals(Action.INTERACTION, action)
                        assertEquals(AwesomeBarFacts.Items.TRENDING_SEARCH_SUGGESTION_CLICKED, item)
                    }
                }

                Mockito.verify(useCase).invoke(eq("firefox focus"), any(), any())
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `GIVEN limit that is less than total number of results WHEN input is changed THEN provider returns suggestions within the limit`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                trendingUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = TrendingSearchProvider(
                fetchClient = HttpURLConnectionClient(),
                privateMode = false,
                searchUseCase = useCase,
                limit = 4,
            )
            provider.setSearchEngine(searchEngine)

            try {
                val suggestions = provider.onInputChanged("")

                assertEquals(4, suggestions.size)

                assertEquals("firefox", suggestions[0].title)
                assertEquals("firefox for mac", suggestions[1].title)
                assertEquals("firefox quantum", suggestions[2].title)
                assertEquals("firefox update", suggestions[3].title)
            } finally {
                server.shutdown()
            }
        }
    }

    private fun getDeviceDesktopIcon(): Bitmap {
        val drawable = R.drawable.mozac_ic_device_desktop_24
        return testContext.getDrawable(drawable)!!.toBitmap()
    }

    private fun getSearchIcon(): Bitmap {
        val drawable = R.drawable.mozac_ic_search_24
        return testContext.getDrawable(drawable)!!.toBitmap()
    }

    @Test
    fun `GIVEN no provider icon WHEN input is changed THEN provider should use engine icon by default`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val engineIcon = getDeviceDesktopIcon()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = engineIcon,
                trendingUrl = server.url("/").toString(),
            )

            val provider = TrendingSearchProvider(
                fetchClient = HttpURLConnectionClient(),
                privateMode = false,
                searchUseCase = mock(),
                limit = 4,
            )
            provider.setSearchEngine(searchEngine)

            try {
                val suggestions = provider.onInputChanged("")
                assertEquals(4, suggestions.size)
                assertTrue(suggestions[0].icon?.sameAs(engineIcon)!!)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `GIVEN a provider icon WHEN input is changed THEN provider should use the given icon`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val engineIcon = getDeviceDesktopIcon()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = engineIcon,
                trendingUrl = server.url("/").toString(),
            )

            val paramIcon = getSearchIcon()

            val provider = TrendingSearchProvider(
                fetchClient = HttpURLConnectionClient(),
                privateMode = false,
                searchUseCase = mock(),
                limit = 4,
                icon = paramIcon,
            )
            provider.setSearchEngine(searchEngine)

            try {
                val suggestions = provider.onInputChanged("")

                assertEquals(4, suggestions.size)
                assertTrue(suggestions[0].icon?.sameAs(paramIcon)!!)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `GIVEN text is not empty WHEN input is changed THEN provider returns an empty list`() = runTest {
        val provider = TrendingSearchProvider(mock(), false, mock())

        val suggestions = provider.onInputChanged("fire")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `GIVEN a search engine that cannot provide trending searches WHEN input is changed THEN provider returns an empty list`() =
        runTest {
            val searchEngine = createSearchEngine(
                name = "Test",
                url = "https://localhost/?q={searchTerms}",
                icon = mock(),
            )

            val provider = TrendingSearchProvider(mock(), false, mock())

            val suggestions = provider.onInputChanged("fire")
            assertEquals(0, suggestions.size)
        }

    @Test
    fun `GIVEN fetch that returns an HTTP error WHEN input is changed THEN provider returns an empty list`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setResponseCode(404).setBody("error"))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider =
                TrendingSearchProvider(HttpURLConnectionClient(), false, useCase)

            try {
                val suggestions = provider.onInputChanged("")
                assertEquals(0, suggestions.size)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `GIVEN fetch that throws an exception WHEN input is changed THEN provider returns an empty list`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                trendingUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val client = object : Client() {
                override fun fetch(request: Request): Response {
                    throw IOException()
                }
            }

            val provider = TrendingSearchProvider(
                fetchClient = client,
                privateMode = false,
                searchUseCase = useCase,
                limit = 4,
            )

            val suggestions = provider.onInputChanged("")
            assertEquals(0, suggestions.size)
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a limit less than 1 WHEN initializing the provider THEN constructor throws an exception`() {
        TrendingSearchProvider(mock(), false, mock(), limit = 0)
    }

    @Test
    fun `GIVEN response has duplicate responses WHEN input is changed THEN provider returns only distinct suggestions`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE_WITH_DUPLICATES))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                trendingUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = TrendingSearchProvider(
                fetchClient = HttpURLConnectionClient(),
                privateMode = false,
                searchUseCase = useCase,
                limit = 11,
            )
            provider.setSearchEngine(searchEngine)

            try {
                val suggestions = provider.onInputChanged("")

                assertEquals(10, suggestions.size)

                assertEquals("firefox", suggestions[0].title)
                assertEquals("firefox for mac", suggestions[1].title)
                assertEquals("firefox quantum", suggestions[2].title)
                assertEquals("firefox update", suggestions[3].title)
                assertEquals("firefox esr", suggestions[4].title)
                assertEquals("firefox focus", suggestions[5].title)
                assertEquals("firefox addons", suggestions[6].title)
                assertEquals("firefox extensions", suggestions[7].title)
                assertEquals("firefox nightly", suggestions[8].title)
                assertEquals("firefox clear cache", suggestions[9].title)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `WHEN input is changed THEN provider calls speculativeConnect for URL of highest scored suggestion`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()
            val engine: Engine = mock()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                trendingUrl = server.url("/").toString(),
            )

            val provider = TrendingSearchProvider(
                fetchClient = HttpURLConnectionClient(),
                privateMode = false,
                searchUseCase = mock(),
                limit = 4,
                engine = engine,
            )
            provider.setSearchEngine(searchEngine)

            try {
                val suggestions = provider.onInputChanged("")
                assertEquals(4, suggestions.size)
                assertEquals("firefox", suggestions[0].title)
                Mockito.verify(engine, Mockito.times(1))
                    .speculativeConnect(server.url("/search?q=firefox").toString())
            } finally {
                server.shutdown()
            }
        }
    }
}
