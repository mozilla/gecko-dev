/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.api

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertResponseIsFailure
import mozilla.components.service.pocket.helpers.assertResponseIsSuccess
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class MarsSpocsEndpointTest {

    private lateinit var client: Client
    private lateinit var endpoint: MarsSpocsEndpoint
    private lateinit var rawEndpoint: MarsSpocsEndpointRaw

    @Before
    fun setUp() {
        client = mock()
        rawEndpoint = mock()

        endpoint = MarsSpocsEndpoint(
            rawEndpoint = rawEndpoint,
        )
    }

    @Test
    fun `WHEN fetching sponsored stories returns a null or empty response THEN return a pocket response failure`() {
        arrayOf(
            null,
            "",
            " ",
        ).forEach { result ->
            whenever(rawEndpoint.getSponsoredStories()).thenReturn(result)

            assertResponseIsFailure(endpoint.getSponsoredStories())
        }
    }

    @Test
    fun `WHEN fetching sponsored stories return a response with a null URL THEN return a pocket response failure`() {
        val response = PocketTestResources.marsSponsoredStoriesNullUrlResponse
        whenever(rawEndpoint.getSponsoredStories()).thenReturn(response)

        assertResponseIsFailure(endpoint.getSponsoredStories())
    }

    @Test
    fun `WHEN fetching sponsored stories return a successful response THEN return a pocket response success`() {
        val response = PocketTestResources.marsSponsoredStoriesJSONResponse
        whenever(rawEndpoint.getSponsoredStories()).thenReturn(response)

        val result = endpoint.getSponsoredStories()

        assertEquals(
            PocketTestResources.marsSpocsResponse,
            (result as? PocketResponse.Success)?.data,
        )
    }

    @Test
    fun `WHEN deleting an user returns a successful response THEN return a pocket response success`() {
        whenever(rawEndpoint.deleteUser()).thenReturn(true)
        assertResponseIsSuccess(endpoint.deleteUser())
    }

    @Test
    fun `WHEN deleting an user returns a failure response THEN return a pocket response failure`() {
        whenever(rawEndpoint.deleteUser()).thenReturn(false)
        assertResponseIsFailure(endpoint.deleteUser())
    }
}
