/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class BundledSearchEnginesStorageTest {
    @Test
    fun `Load search engines for en-US from assets`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)

        val engines = storage.load(RegionState("US", "US"), Locale.Builder().setLanguage("en").setRegion("US").build())
        val searchEngines = engines.list

        assertEquals(5, searchEngines.size)
    }

    @Test
    fun `Load search engines for all known locales without region`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val locales = Locale.getAvailableLocales()
        assertTrue(locales.isNotEmpty())

        for (locale in locales) {
            val engines = storage.load(RegionState.Default, locale)
            assertTrue(engines.list.isNotEmpty())
            assertFalse(engines.defaultSearchEngineId.isEmpty())
        }
    }

    @Test
    fun `Load search engines for de-DE with global US region override`() = runTest {
        // Without region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState.Default, Locale.Builder().setLanguage("de").setRegion("DE").build())
            val searchEngines = engines.list

            assertEquals(7, searchEngines.size)
            assertContainsSearchEngine("google-b-m", searchEngines)
            assertContainsNotSearchEngine("google-2018", searchEngines)
        }
        // With region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState("US", "US"), Locale.Builder().setLanguage("de").setRegion("DE").build())
            val searchEngines = engines.list

            assertEquals(7, searchEngines.size)
            assertContainsSearchEngine("google-b-1-m", searchEngines)
            assertContainsNotSearchEngine("google", searchEngines)
        }
    }

    @Test
    fun `Load search engines for en-US with local RU region override`() = runTest {
        // Without region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState.Default, Locale.Builder().setLanguage("en").setRegion("US").build())
            val searchEngines = engines.list

            println("searchEngines = $searchEngines")
            assertEquals(5, searchEngines.size)
            assertContainsNotSearchEngine("yandex-en", searchEngines)
        }
        // With region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState("RU", "RU"), Locale.Builder().setLanguage("en").setRegion("US").build())
            val searchEngines = engines.list

            println("searchEngines = $searchEngines")
            assertEquals(4, searchEngines.size)
            assertContainsSearchEngine("google-com-nocodes", searchEngines)
            assertContainsNotSearchEngine("yandex-en", searchEngines)
        }
    }

    @Test
    fun `Load search engines for zh-CN_CN locale with searchDefault override`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState("CN", "CN"), Locale.Builder().setLanguage("zh").setRegion("CN").build())
        val searchEngines = engines.list

        // visibleDefaultEngines: ["google-b-m", "bing", "baidu", "ddg", "wikipedia-zh-CN"]
        // searchOrder (default): ["Google", "Bing"]

        assertEquals(
            listOf("google-b-m", "bing", "baidu", "ddg", "wikipedia-zh-CN"),
            searchEngines.map { it.id },
        )

        // searchDefault: "百度"
        val default = searchEngines.find { it.id == engines.defaultSearchEngineId }
        assertNotNull(default)
        assertEquals("baidu", default!!.id)
    }

    @Test
    fun `Load search engines for ru_RU locale with engines not in searchOrder`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState("RU", "RU"), Locale.Builder().setLanguage("ru").setRegion("RU").build())
        val searchEngines = engines.list

        assertEquals(
            listOf("google-com-nocodes", "ddg", "wikipedia-ru"),
            searchEngines.map { it.id },
        )

        // searchDefault: "Google"
        val default = searchEngines.find { it.id == engines.defaultSearchEngineId }
        assertNotNull(default)
        assertEquals("google-com-nocodes", default!!.id)
    }

    @Test
    fun `Load search engines for trs locale with non-google initial engines and no default`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState.Default, Locale.Builder().setLanguage("trs").build())
        val searchEngines = engines.list

        // visibleDefaultEngines: ["google-b-m", "bing", "ddg", "wikipedia-es"]
        // searchOrder (default): ["Google", "Bing"]

        assertEquals(
            listOf("google-b-m", "bing", "ddg", "wikipedia-es"),
            searchEngines.map { it.id },
        )

        // searchDefault (default): "Google"
        val default = searchEngines.find { it.id == engines.defaultSearchEngineId }
        assertNotNull(default)
        assertEquals("google-b-m", default!!.id)
    }

    @Test
    fun `Load search engines for locale not in configuration`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState.Default, Locale.forLanguageTag("xx-XX"))
        val searchEngines = engines.list

        assertEquals(4, searchEngines.size)
    }

    private fun assertContainsSearchEngine(identifier: String, searchEngines: List<SearchEngine>) {
        searchEngines.forEach {
            if (identifier == it.id) {
                return
            }
        }
        throw AssertionError("Search engine $identifier not in list")
    }

    private fun assertContainsNotSearchEngine(identifier: String, searchEngines: List<SearchEngine>) {
        searchEngines.forEach {
            if (identifier == it.id) {
                throw AssertionError("Search engine $identifier in list")
            }
        }
    }

    @Test
    fun `Verify values of Google search engine`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)

        val engines = storage.load(RegionState("US", "US"), Locale.Builder().setLanguage("en").setRegion("US").build())
        val searchEngines = engines.list

        assertEquals(5, searchEngines.size)

        val google = searchEngines.find { it.name == "Google" }
        assertNotNull(google!!)

        assertEquals("google-b-1-m", google.id)
        assertEquals("Google", google.name)
        assertEquals(SearchEngine.Type.BUNDLED, google.type)
        assertNotNull(google.icon)
        assertEquals("https://www.google.com/complete/search?client=firefox&q={searchTerms}", google.suggestUrl)
        assertTrue(google.resultUrls.isNotEmpty())
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in US`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "US",
            localeLang = "en",
            localeCountry = "US",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf("google-b-vv", "bing", "ddg", "ebay", "wikipedia"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in Austria`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "AT",
            localeLang = "de",
            localeCountry = "AT",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf(
                "google-b-vv",
                "bing",
                "ddg",
                "ecosia",
                "qwant",
                "wikipedia-de",
                "ebay-at",
            ),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in Spain`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "ES",
            localeLang = "es",
            localeCountry = "ES",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf("google-b-vv", "bing", "ddg", "wikipedia-es", "ebay-es"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in Italy`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "IT",
            localeLang = "it",
            localeCountry = "IT",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf("google-b-vv", "bing", "ddg", "wikipedia-it", "ebay-it"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in Germany`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "DE",
            localeLang = "de",
            localeCountry = "DE",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf(
                "google-b-vv",
                "bing",
                "ddg",
                "ecosia",
                "qwant",
                "wikipedia-de",
                "ebay-de",
            ),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in France`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "FR",
            localeLang = "fr",
            localeCountry = "FR",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf(
                "google-b-vv",
                "bing",
                "ddg",
                "qwant",
                "wikipedia-fr",
                "ebay-fr",
            ),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in Mexico`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "MX",
            localeLang = "es",
            localeCountry = "MX",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf(
                "google-b-vv",
                "bing",
                "ddg",
                "mercadolibre-mx",
                "wikipedia-es",
            ),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in Colombia`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "CO",
            localeLang = "es",
            localeCountry = "CO",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf("google-b-vv", "bing", "ddg", "wikipedia"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in an unknown country and language`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "MEEP",
            localeLang = "beepbeep",
            localeCountry = "MEEP",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf("google-b-vv", "bing", "ddg", "wikipedia"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in Russia in russian`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "RU",
            localeLang = "ru",
            localeCountry = "RU",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf("google-com-nocodes", "ddg", "wikipedia-ru"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for vivo-001 distributions in USA in russian`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "US",
            localeLang = "ru",
            localeCountry = "US",
            distribution = "vivo-001",
        )

        assertEquals(
            listOf("google-b-vv", "ddg", "wikipedia-ru"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for mozilla distribution in USA`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "US",
            localeLang = "en",
            localeCountry = "US",
            distribution = "Mozilla",
        )

        assertEquals(
            listOf("google-b-1-m", "bing", "ddg", "ebay", "wikipedia"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for mozilla distribution in Germany in german`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "DE",
            localeLang = "de",
            localeCountry = "DE",
            distribution = "Mozilla",
        )

        assertEquals(
            listOf(
                "google-b-m",
                "bing",
                "ddg",
                "ecosia",
                "qwant",
                "wikipedia-de",
                "ebay-de",
            ),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for mozilla distribution in Russia in russian`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "RU",
            localeLang = "ru",
            localeCountry = "RU",
            distribution = "Mozilla",
        )

        assertEquals(
            listOf("google-com-nocodes", "ddg", "wikipedia-ru"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for mozilla distribution in Germany in russian`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "DE",
            localeLang = "ru",
            localeCountry = "DE",
            distribution = "Mozilla",
        )

        assertEquals(
            listOf("google-b-m", "ddg", "wikipedia-ru"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for mozilla distribution in USA in russian`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "US",
            localeLang = "ru",
            localeCountry = "US",
            distribution = "Mozilla",
        )

        assertEquals(
            listOf("google-b-1-m", "ddg", "wikipedia-ru"),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for unknown distribution in USA`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "US",
            localeLang = "en",
            localeCountry = "US",
            distribution = "unknown",
        )

        assertEquals(
            listOf(
                "google-b-1-m",
                "bing",
                "ddg",
                "ebay",
                "wikipedia",
            ),
            searchEngines,
        )
    }

    @Test
    fun `Verify search engines for unknown distribution in an unknown country and language`() = runTest {
        val searchEngines = loadSearchEngines(
            region = "MEEP",
            localeLang = "beepbeep",
            localeCountry = "MEEP",
            distribution = "unknown",
        )

        assertEquals(
            listOf("google-b-m", "bing", "ddg", "wikipedia"),
            searchEngines,
        )
    }

    private suspend fun loadSearchEngines(
        region: String,
        localeLang: String,
        localeCountry: String,
        distribution: String,
    ): List<String> {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(
            region = RegionState(region, region),
            locale = Locale.forLanguageTag("$localeLang-$localeCountry"),
            distribution = distribution,
        )
        return engines.list.map { it.id }
    }
}
