/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.api

import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertResponseIsFailure
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class ContentRecommendationsEndpointTest {

    private lateinit var client: Client
    private lateinit var endpoint: ContentRecommendationsEndpoint
    private lateinit var rawEndpoint: ContentRecommendationEndpointRaw

    @Before
    fun setUp() {
        client = mock()
        rawEndpoint = mock()

        endpoint = ContentRecommendationsEndpoint(
            rawEndpoint = rawEndpoint,
        )
    }

    @Test
    fun `WHEN fetching content recommendations returns a null or empty response THEN return a pocket response failure`() {
        arrayOf(
            null,
            "",
        ).forEach { result ->
            whenever(rawEndpoint.getContentRecommendations()).thenReturn(result)

            assertResponseIsFailure(endpoint.getContentRecommendations())
        }
    }

    @Test
    fun `WHEN fetching content recommendations return a response with a null URL THEN return a pocket response failure`() {
        val response = PocketTestResources.contentRecommendationsNullUrlResponse
        whenever(rawEndpoint.getContentRecommendations()).thenReturn(response)

        assertResponseIsFailure(endpoint.getContentRecommendations())
    }

    @Test
    fun `WHEN fetching content recommendations return a successful response THEN return a pocket response success`() {
        val response = PocketTestResources.contentRecommendationsJSONResponse
        whenever(rawEndpoint.getContentRecommendations()).thenReturn(response)

        val result = endpoint.getContentRecommendations()

        assertEquals(
            PocketTestResources.contentRecommendationsResponse,
            (result as? PocketResponse.Success)?.data,
        )
    }
}
