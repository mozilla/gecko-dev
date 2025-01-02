/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.mars

import android.text.format.DateUtils
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.any
import mozilla.components.support.test.file.loadResourceAsString
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.File
import java.util.Date

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class MarsTopSitesProviderTest {

    @Test
    fun `GIVEN a successful status response WHEN top sites are fetched THEN return top sites from the response`() = runTest {
        val provider = MarsTopSitesProvider(
            context = testContext,
            client = getClient(),
            requestConfig = getRequestConfig(),
        )
        val topSites = provider.getTopSites()

        var topSite = topSites.first()

        assertEquals(2, topSites.size)
        assertNull(topSite.id)
        assertEquals("Firefox", topSite.title)
        assertEquals("https://firefox.com", topSite.url)
        assertEquals("https://firefox.com/click", topSite.clickUrl)
        assertEquals("https://test.com/image1.jpg", topSite.imageUrl)
        assertEquals("https://firefox.com/impression", topSite.impressionUrl)

        topSite = topSites.last()

        assertNull(topSite.id)
        assertEquals("Mozilla", topSite.title)
        assertEquals("https://mozilla.com", topSite.url)
        assertEquals("https://mozilla.com/click", topSite.clickUrl)
        assertEquals("https://test.com/image2.jpg", topSite.imageUrl)
        assertEquals("https://mozilla.com/impression", topSite.impressionUrl)
    }

    @Test
    fun `GIVEN a cache configuration is allowed and not expired WHEN top sites are fetched THEN read from the disk cache`() = runTest {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(),
                requestConfig = getRequestConfig(),
            ),
        )

        provider.getTopSites(allowCache = false)
        verify(provider, never()).readFromDiskCache()

        whenever(provider.isCacheExpired()).thenReturn(true)
        provider.getTopSites(allowCache = true)
        verify(provider, never()).readFromDiskCache()

        whenever(provider.isCacheExpired()).thenReturn(false)
        provider.getTopSites(allowCache = true)
        verify(provider).readFromDiskCache()
    }

    @Test
    fun `GIVEN a 500 status response WHEN top sites are fetched AND cached top sites are not valid THEN return no top sites`() = runTest {
        val provider = MarsTopSitesProvider(
            context = testContext,
            client = getClient(status = 500),
            requestConfig = getRequestConfig(),
        )
        val topSites = provider.getTopSites()

        assertEquals(0, topSites.size)
    }

    @Test
    fun `GIVEN a 500 status response WHEN top sites are fetched AND cached top sites are valid THEN return the cached top sites`() = runTest {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(status = 500),
                requestConfig = getRequestConfig(),
            ),
        )
        val topSites = mock<List<TopSite.Provided>>()

        whenever(provider.isCacheExpired()).thenReturn(false)
        whenever(provider.readFromDiskCache()).thenReturn(topSites)

        assertEquals(topSites, provider.getTopSites())
    }

    @Test
    fun `GIVEN a cache max age is specified WHEN top sites are fetched THEN the cache max age is correctly set`() = runTest {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(),
                requestConfig = getRequestConfig(),
                maxCacheAgeInSeconds = 60L,
            ),
        )

        provider.getTopSites()

        verify(provider).writeToDiskCache(any())
        assertEquals(
            provider.cacheState.localCacheMaxAge,
            provider.cacheState.getCacheMaxAge(),
        )
        assertFalse(provider.isCacheExpired())
    }

    @Test
    fun `GIVEN cache max age is not specified WHEN top sites are fetched THEN the cache is expired`() = runTest {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(),
                requestConfig = getRequestConfig(),
            ),
        )

        provider.getTopSites()

        verify(provider, never()).writeToDiskCache(any())
        assertTrue(provider.isCacheExpired())
    }

    @Test
    fun `WHEN the base cache file getter is called THEN return existing base cache file`() {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(),
                requestConfig = getRequestConfig(),
            ),
        )
        val file = File(testContext.filesDir, CACHE_FILE_NAME)

        file.createNewFile()

        assertTrue(file.exists())

        val cacheFile = provider.getBaseCacheFile()

        assertTrue(cacheFile.exists())
        assertEquals(file.name, cacheFile.name)

        assertTrue(file.delete())
        assertFalse(cacheFile.exists())
    }

    @Test
    fun `WHEN the cache expiration is checked THEN return whether the cache is expired`() {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(),
                requestConfig = getRequestConfig(),
            ),
        )

        provider.cacheState = CacheState(isCacheValid = false)
        assertTrue(provider.isCacheExpired())

        provider.cacheState = CacheState(
            isCacheValid = true,
            localCacheMaxAge = Date().time - 60 * DateUtils.MINUTE_IN_MILLIS,
        )
        assertTrue(provider.isCacheExpired())

        provider.cacheState = CacheState(
            isCacheValid = true,
            localCacheMaxAge = Date().time + 60 * DateUtils.MINUTE_IN_MILLIS,
        )
        assertFalse(provider.isCacheExpired())
    }

    @Test
    fun `GIVEN cache is not expired WHEN top sites are refreshed THEN do nothing`() = runTest {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(),
                requestConfig = getRequestConfig(),
                maxCacheAgeInSeconds = 600,
            ),
        )

        whenever(provider.isCacheExpired()).thenReturn(false)
        provider.refreshTopSitesIfCacheExpired()
        verify(provider, never()).getTopSites(allowCache = false)
    }

    @Test
    fun `GIVEN cache is expired WHEN top sites are refreshed THEN fetch and write new response to cache`() = runTest {
        val provider = spy(
            MarsTopSitesProvider(
                context = testContext,
                client = getClient(),
                requestConfig = getRequestConfig(),
                maxCacheAgeInSeconds = 600,
            ),
        )

        whenever(provider.isCacheExpired()).thenReturn(true)

        provider.refreshTopSitesIfCacheExpired()

        verify(provider).getTopSites(allowCache = false)
        verify(provider).writeToDiskCache(any())
    }

    private fun getRequestConfig(
        contextId: String = "contextId",
        userAgent: String = "userAgent",
        placements: List<Placement> = listOf(),
    ) = MarsTopSitesRequestConfig(
        contextId = contextId,
        userAgent = userAgent,
        placements = placements,
    )

    private fun getClient(
        jsonResponse: String = loadResourceAsString("/mars/tiles.json"),
        status: Int = 200,
    ): Client {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()

        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(status)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        return mockedClient
    }
}
