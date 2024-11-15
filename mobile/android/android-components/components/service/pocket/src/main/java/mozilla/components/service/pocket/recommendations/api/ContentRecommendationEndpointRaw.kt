/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.api

import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Request.Body
import mozilla.components.concept.fetch.Request.Method
import mozilla.components.service.pocket.ContentRecommendationsRequestConfig
import mozilla.components.service.pocket.ext.fetchBodyOrNull
import mozilla.components.service.pocket.recommendations.api.ContentRecommendationsEndpoint.Companion.newInstance
import mozilla.components.service.pocket.stories.api.PocketEndpoint.Companion.newInstance
import org.json.JSONObject

@VisibleForTesting
internal const val ENDPOINT_URL =
    "https://merino.services.mozilla.com/api/v1/curated-recommendations"

@VisibleForTesting
internal const val REQUEST_BODY_LOCALE_KEY = "locale"

@VisibleForTesting
internal const val REQUEST_BODY_REGION_KEY = "region"

@VisibleForTesting
internal const val REQUEST_BODY_COUNT_KEY = "count"

@VisibleForTesting
internal const val REQUEST_BODY_TOPICS_KEY = "topics"

/**
 * Performs requests to the retrieve the content recommendations from the [endpointURL] server
 * with the provided [config].
 *
 * See https://merino.services.mozilla.com/docs#/default/curated_content_api_v1_curated_recommendations_post
 * for documentation of the API endpoint.
 *
 * @see [newInstance] to retrieve an instance.
 *
 * @property client The [Client] used for interacting with the content recommendations HTTP API.
 * @property config Configuration for content recommendations request.
 * @property endpointURL The url of the endpoint to fetch from. Defaults to [ENDPOINT_URL].
 */
internal class ContentRecommendationEndpointRaw internal constructor(
    @get:VisibleForTesting internal val client: Client,
    @get:VisibleForTesting internal val config: ContentRecommendationsRequestConfig,
    private val endpointURL: String = ENDPOINT_URL,
) {
    /**
     * Fetches from the content recommendation [endpointURL] and returns the content
     * recommendations as a JSON string or null.
     *
     * @return The content recommendations as a raw JSON string or null on error.
     */
    @WorkerThread
    fun getContentRecommendations(): String? = makeRequest()

    /**
     * Retrieves the content recommendations from the endpoint server.
     *
     * @return The requested JSON as a String or null on error.
     */
    @WorkerThread // synchronous request.
    private fun makeRequest(): String? {
        val request = Request(
            url = endpointURL,
            method = Method.POST,
            headers = MutableHeaders(
                "Content-Type" to "application/json; charset=UTF-8",
            ),
            body = getRequestBody(),
            conservative = true,
        )
        return client.fetchBodyOrNull(request)
    }

    private fun getRequestBody(): Body {
        val params = mapOf(
            REQUEST_BODY_LOCALE_KEY to config.locale,
            REQUEST_BODY_REGION_KEY to config.region,
            REQUEST_BODY_COUNT_KEY to config.count,
            REQUEST_BODY_TOPICS_KEY to config.topics,
        )

        return Body(JSONObject(params).toString().byteInputStream())
    }

    companion object {
        /**
         * Returns a new instance of [ContentRecommendationEndpointRaw].
         *
         * @param client The HTTP [Client] to use for network requests.
         * @param config Configuration for content recommendations request.
         */
        fun newInstance(
            client: Client,
            config: ContentRecommendationsRequestConfig,
        ): ContentRecommendationEndpointRaw {
            return ContentRecommendationEndpointRaw(client, config)
        }
    }
}
