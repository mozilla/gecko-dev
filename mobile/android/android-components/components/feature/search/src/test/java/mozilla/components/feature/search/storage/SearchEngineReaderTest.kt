/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import android.util.AtomicFile
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.appservices.search.SearchEngineClassification
import mozilla.appservices.search.SearchEngineDefinition
import mozilla.appservices.search.SearchEngineUrl
import mozilla.appservices.search.SearchEngineUrls
import mozilla.appservices.search.SearchUrlParam
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.feature.search.icons.AttachmentModel
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class SearchEngineReaderTest {
    @Test
    fun `SearchEngineReader can read from a file`() {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            inputEncoding = "ISO-8859-1",
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )
        val readSearchEngine = saveAndLoadSearchEngine(searchEngine)

        assertEquals(searchEngine.id, readSearchEngine.id)
        assertEquals(searchEngine.name, readSearchEngine.name)
        assertEquals(searchEngine.inputEncoding, readSearchEngine.inputEncoding)
        assertEquals(searchEngine.type, readSearchEngine.type)
        assertEquals(searchEngine.resultUrls, readSearchEngine.resultUrls)
        assertTrue(readSearchEngine.isGeneral)
    }

    @Test(expected = IOException::class)
    fun `Parsing not existing file will throw exception`() {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )
        val reader = SearchEngineReader(type = SearchEngine.Type.CUSTOM)
        val invalidFile = AtomicFile(File("", ""))
        reader.loadFile(searchEngine.id, invalidFile)
    }

    @Test
    fun `WHEN SearchEngineReader is loading bundled search engines from a file THEN the correct SearchEngine properties are parsed`() {
        for (id in GENERAL_SEARCH_ENGINE_IDS + setOf("mozilla", "wikipedia")) {
            val searchEngine = SearchEngine(
                id = id,
                name = "example",
                icon = mock(),
                type = SearchEngine.Type.BUNDLED,
                resultUrls = listOf("https://www.example.com/search"),
            )
            val readSearchEngine = saveAndLoadSearchEngine(searchEngine)

            assertEquals(searchEngine.id, readSearchEngine.id)
            assertEquals(searchEngine.name, readSearchEngine.name)
            assertEquals(searchEngine.type, readSearchEngine.type)
            assertEquals(searchEngine.resultUrls, readSearchEngine.resultUrls)
            assertEquals(id in GENERAL_SEARCH_ENGINE_IDS, readSearchEngine.isGeneral)
        }
    }

    @Test
    fun `GIVEN a search engine with a trending URL WHEN SearchEngineReader loads the search engine from a file THEN the trending URL is correctly parsed`() {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            trendingUrl = "https://www.example.com/complete/search?client=firefox&channel=ftr&q={searchTerms}",
        )
        val readSearchEngine = saveAndLoadSearchEngine(searchEngine)

        assertEquals(searchEngine.id, readSearchEngine.id)
        assertEquals(searchEngine.name, readSearchEngine.name)
        assertEquals(searchEngine.type, readSearchEngine.type)
        assertEquals(searchEngine.trendingUrl, readSearchEngine.trendingUrl)
    }

    private fun saveAndLoadSearchEngine(searchEngine: SearchEngine): SearchEngine {
        val storage = CustomSearchEngineStorage(testContext)
        val writer = SearchEngineWriter()
        val reader = SearchEngineReader(type = searchEngine.type)
        val file = storage.getSearchFile(searchEngine.id)

        writer.saveSearchEngineXML(searchEngine, file)

        return reader.loadFile(searchEngine.id, file)
    }

    @Test
    fun `GIVEN {partnerCode} in value of a SearchURLParam THEN it is replaced by actual partnerCode`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = sampleAttachmentModelData()

        searchEngineDefinition.urls.search.params +=
            SearchUrlParam(
                name = "client",
                value = "{partnerCode}",
                enterpriseValue = null,
                experimentConfig = null,
            )
        searchEngineDefinition.partnerCode = "test-firefox-code"
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        assertEquals("https://www.google.com/search?client=test-firefox-code", searchEngine.resultUrls[0])
    }

    @Test
    fun `Given null value of a SearchURLParam THEN it is not appended to the URL`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = sampleAttachmentModelData()
        searchEngineDefinition.urls.search.params +=
            SearchUrlParam(
                name = "channel",
                value = null,
                enterpriseValue = null,
                experimentConfig = null,
            )
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        assertEquals("https://www.google.com/search", searchEngine.resultUrls[0])
    }

    @Test
    fun `GIVEN searchTermParamName in SearchEngineUrl THEN add a new param with name searchTermParamName and value {searchTerms}`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = sampleAttachmentModelData()
        searchEngineDefinition.urls.search.searchTermParamName = "test"
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        assertEquals("https://www.google.com/search?test=%7BsearchTerms%7D", searchEngine.resultUrls[0])
    }

    @Test
    fun `GIVEN searchTermParamName in SearchEngineUrl and {searchTerms} in base url THEN don't add a new param with value {searchTerms}`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = sampleAttachmentModelData()
        searchEngineDefinition.urls.search.searchTermParamName = "test"
        searchEngineDefinition.urls.search.base = "https://www.google.com/q={searchTerms}"
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        assertEquals("https://www.google.com/q={searchTerms}", searchEngine.resultUrls[0])
    }

    @Test
    fun `GIVEN search, suggest and trending URLs THEN they are correctly parsed`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = sampleAttachmentModelData()
        searchEngineDefinition.urls.search.base = "https://www.google.com/search"
        searchEngineDefinition.urls.search.params += SearchUrlParam(name = "search-test-name", value = "search-test-value", enterpriseValue = null, experimentConfig = null)
        searchEngineDefinition.urls.search.searchTermParamName = "test"

        searchEngineDefinition.urls.suggestions = SearchEngineUrl(
            base = "https://www.google.com/suggest/search",
            method = "GET",
            params = listOf(
                SearchUrlParam(
                    name = "suggestions-test-name",
                    value = "suggestions-test-value",
                    enterpriseValue = null,
                    experimentConfig = null,
                ),
            ),
            searchTermParamName = "test2",
        )

        searchEngineDefinition.urls.trending = SearchEngineUrl(
            base = "https://www.google.com/trending/search",
            method = "GET",
            params = listOf(
                SearchUrlParam(
                    name = "trending-test-name",
                    value = "trending-test-value",
                    enterpriseValue = null,
                    experimentConfig = null,
                ),
            ),
            searchTermParamName = "test3",
        )

        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)

        assertEquals(searchEngineDefinition.identifier, searchEngine.id)
        assertEquals(searchEngineDefinition.name, searchEngine.name)
        assertEquals(searchEngineDefinition.charset, searchEngine.inputEncoding)

        assertEquals("https://www.google.com/search?search-test-name=search-test-value&test=%7BsearchTerms%7D", searchEngine.resultUrls[0])
        assertEquals("https://www.google.com/suggest/search?suggestions-test-name=suggestions-test-value&test2=%7BsearchTerms%7D", searchEngine.suggestUrl)
        assertEquals("https://www.google.com/trending/search?trending-test-name=trending-test-value&test3=%7BsearchTerms%7D", searchEngine.trendingUrl)
    }

    @Test
    fun `GIVEN null name THEN throw exception`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = sampleAttachmentModelData()
        searchEngineDefinition.name = ""
        val exception = assertThrows(IllegalArgumentException::class.java) {
            reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        }
        assertEquals("Search engine name cannot be empty", exception.message)
    }

    @Test
    fun `GIVEN null identifier THEN throw exception`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = sampleAttachmentModelData()
        searchEngineDefinition.identifier = ""

        val exception = assertThrows(IllegalArgumentException::class.java) {
            reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        }
        assertEquals("Search engine identifier cannot be empty", exception.message)
    }

    @Test
    fun `GIVEN valid jpeg image THEN readImageAPI decodes it`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = AttachmentModel(
            filename = "test",
            mimetype = "image/jpeg",
            location = "main-workspace/search-config-icons/d0e5c407-7b88-4030-8870-f44498141ec7.jpg",
            hash = "test",
            size = 100u,
        )
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        assertNotNull(searchEngine.icon)
    }

    @Test
    fun `GIVEN valid png image THEN readImageAPI decodes it`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = AttachmentModel(
            filename = "test",
            mimetype = "image/png",
            location = "main-workspace/search-config-icons/bcf53867-215e-40f1-9a6e-bc4c5768c5c4.png",
            hash = "test",
            size = 100u,
        )
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        assertNotNull(searchEngine.icon)
    }

    @Test
    fun `GIVEN valid ico image THEN readImageAPI decodes it`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = AttachmentModel(
            filename = "test",
            mimetype = "image/x-icon",
            location = "main-workspace/search-config-icons/5ed361f5-5b94-4899-896a-747d107f7392.ico",
            hash = "test",
            size = 100u,
        )
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        assertNotNull(searchEngine.icon)
    }

    @Test
    fun `GIVEN invalid image mimetype THEN readImageAPI throws exception`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = AttachmentModel(
            filename = "test",
            mimetype = "image/gif",
            location = "main-workspace/search-config-icons/5ed361f5-5b94-4899-896a-747d107f7392.ico",
            hash = "test",
            size = 100u,
        )

        val exception = assertThrows(IllegalArgumentException::class.java) {
            reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        }
        assertEquals("Unsupported image type: image/gif", exception.message)
    }

    @Test
    fun `GIVEN invalid image location THEN readImageAPI throws exception`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()

        val attachmentModel = AttachmentModel(
            filename = "test",
            mimetype = "image/png",
            location = "test",
            hash = "test",
            size = 100u,
        )
        val exception = assertThrows(IllegalArgumentException::class.java) {
            reader.loadStreamAPI(searchEngineDefinition, attachmentModel)
        }
        assertEquals("Failed to read image from location: https://firefox-settings-attachments.cdn.mozilla.net/test", exception.message)
    }

    @Test
    fun `GIVEN specific icons url prefix THEN readImageAPI reads from correct url`() {
        val reader = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
        val searchEngineDefinition = sampleSearchEngineDefinitionData()
        val attachmentModel = AttachmentModel(
            filename = "test",
            mimetype = "image/x-icon",
            location = "main-workspace/search-config-icons/53f837f7-abf4-463d-b8a7-d4526864a7de.ico",
            hash = "test",
            size = 100u,
        )
        val searchEngine = reader.loadStreamAPI(searchEngineDefinition, attachmentModel, "https://firefox-settings-attachments.cdn.allizom.org/")
        assertNotNull(searchEngine.icon)
    }

    private fun sampleSearchEngineDefinitionData(): SearchEngineDefinition {
        val engineDefinition = SearchEngineDefinition(
            aliases = listOf("google"),
            charset = "UTF-8",
            classification = SearchEngineClassification.GENERAL,
            identifier = "google",
            name = "Google",
            optional = false,
            partnerCode = "firefox-b-m",
            telemetrySuffix = "b-m",
            urls = SearchEngineUrls(
                search = SearchEngineUrl(
                    base = "https://www.google.com/search",
                    method = "GET",
                    params = emptyList(),
                    searchTermParamName = null,
                ),
                suggestions = null,
                trending = null,
                searchForm = null,
            ),
            orderHint = null,
            clickUrl = null,
        )
        return engineDefinition
    }

    private fun sampleAttachmentModelData(): AttachmentModel {
        return AttachmentModel(
            filename = "test",
            mimetype = "image/jpeg",
            location = "main-workspace/search-config-icons/d0e5c407-7b88-4030-8870-f44498141ec7.jpg",
            hash = "test",
            size = 100u,
        )
    }
}
