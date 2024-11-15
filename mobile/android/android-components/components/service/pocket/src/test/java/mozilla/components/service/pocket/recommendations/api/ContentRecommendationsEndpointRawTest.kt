/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.api

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.ContentRecommendationsRequestConfig
import mozilla.components.service.pocket.helpers.MockResponses
import mozilla.components.service.pocket.helpers.assertRequestParams
import mozilla.components.service.pocket.helpers.assertResponseIsClosed
import mozilla.components.service.pocket.helpers.assertSuccessfulRequestReturnsResponseBody
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class ContentRecommendationsEndpointRawTest {

    private lateinit var endpoint: ContentRecommendationEndpointRaw
    private lateinit var client: Client

    private lateinit var errorResponse: Response
    private lateinit var successResponse: Response
    private lateinit var defaultResponse: Response

    @Before
    fun setUp() {
        errorResponse = MockResponses.getError()
        successResponse = MockResponses.getSuccess()
        defaultResponse = errorResponse

        client = mock<Client>().also {
            whenever(it.fetch(any())).thenReturn(defaultResponse)
        }

        endpoint = ContentRecommendationEndpointRaw(
            client = client,
            config = ContentRecommendationsRequestConfig(),
        )
    }

    @Test
    fun `WHEN requesting content recommendations with a custom request config THEN ensure the correct request parameters are used`() {
        val locale = "en"
        val region = "ca"
        val count = 10
        val topics = listOf("business", "health")
        val customEndpoint = ContentRecommendationEndpointRaw(
            client = client,
            config = ContentRecommendationsRequestConfig(
                locale = locale,
                region = region,
                count = count,
                topics = topics,
            ),
        )

        assertRequestParams(
            client = client,
            makeRequest = {
                customEndpoint.getContentRecommendations()
            },
            assertParams = { request ->
                assertEquals(ENDPOINT_URL, request.url)
                assertEquals(Request.Method.POST, request.method)
                assertTrue(request.conservative)

                request.headers!!.first {
                    it.name.equals("Content-Type", true)
                }.value.contains("application/json", true)

                val requestBody = JSONObject(
                    request.body!!.useStream {
                        it.bufferedReader().readText()
                    },
                )
                assertEquals(locale, requestBody.get(REQUEST_BODY_LOCALE_KEY))
                assertEquals(region, requestBody.get(REQUEST_BODY_REGION_KEY))
                assertEquals(count, requestBody.get(REQUEST_BODY_COUNT_KEY))
                assertEquals(
                    topics.joinToString(separator = ",", prefix = "[", postfix = "]") { "\"$it\"" },
                    requestBody.get(REQUEST_BODY_TOPICS_KEY).toString(),
                )
            },
        )
    }

    @Test
    fun `WHEN requesting content recommendations and the client throws an IOException THEN null is returned`() {
        whenever(client.fetch(any())).thenThrow(IOException::class.java)
        assertNull(endpoint.getContentRecommendations())
    }

    @Test
    fun `WHEN requesting content recommendations and the response is null THEN null is returned`() {
        whenever(client.fetch(any())).thenReturn(null)
        assertNull(endpoint.getContentRecommendations())
    }

    @Test
    fun `WHEN requesting content recommendations and the response is a failure THEN null is returned`() {
        whenever(client.fetch(any())).thenReturn(errorResponse)
        assertNull(endpoint.getContentRecommendations())
    }

    @Test
    fun `WHEN requesting content recommendations and the response is a success THEN the response body is returned`() {
        assertSuccessfulRequestReturnsResponseBody(client, endpoint::getContentRecommendations)
    }

    @Test
    fun `WHEN requesting content recommendations and a success or error response is received THEN response is closed`() {
        assertResponseIsClosed(client, successResponse) {
            endpoint.getContentRecommendations()
        }

        assertResponseIsClosed(client, errorResponse) {
            endpoint.getContentRecommendations()
        }
    }
}
