/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.api

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.concept.fetch.Headers.Values.CONTENT_TYPE_APPLICATION_JSON
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Request.Body
import mozilla.components.concept.fetch.Request.Method
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.service.pocket.BuildConfig
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.mars.api.MarsSpocsEndpointRaw.Companion.newInstance
import mozilla.components.service.pocket.recommendations.api.ContentRecommendationEndpointRaw.Companion.newInstance
import mozilla.components.service.pocket.recommendations.api.ContentRecommendationsEndpoint.Companion.newInstance
import mozilla.components.service.pocket.stories.api.PocketEndpoint.Companion.newInstance
import mozilla.components.support.base.ext.fetchBodyOrNull
import org.json.JSONObject
import java.io.IOException

internal const val MARS_ENDPOINT_BASE_URL = "https://ads.mozilla.org/v1/"
internal const val MARS_ENDPOINT_STAGING_BASE_URL = "https://ads.allizom.org/v1/"
internal const val MARS_ENDPOINT_ADS_PATH = "ads"
internal const val MARS_ENDPOINT_DELETE_USER_PATH = "delete_user"

internal const val REQUEST_BODY_CONTEXT_ID_KEY = "context_id"
internal const val REQUEST_BODY_PLACEMENTS_KEY = "placements"
internal const val REQUEST_BODY_PLACEMENT_KEY = "placement"
internal const val REQUEST_BODY_COUNT_KEY = "count"

/**
 * Performs requests to retrieve the sponsored stories from the MARS endpoint with the provided
 * [config].
 *
 * @see [newInstance] to retrieve an instance.
 *
 * @property client The [Client] used for interacting with the MARS HTTP API.
 * @property config Configuration for the sponsored stories request.
 */
internal class MarsSpocsEndpointRaw internal constructor(
    @get:VisibleForTesting internal val client: Client,
    @get:VisibleForTesting internal val config: MarsSpocsRequestConfig,
) {
    /**
     * Makes a request to the MARS endpoint and returns the sponsored stories as a JSON string or
     * null.
     *
     * @return The sponsored stories as a raw JSON string or null on error.
     */
    @WorkerThread
    fun getSponsoredStories(): String? {
        val url = Uri.Builder()
            .encodedPath(baseUrl + MARS_ENDPOINT_ADS_PATH)
            .build()
            .toString()
        val request = Request(
            url = url,
            method = Method.POST,
            headers = getSponsoredStoriesRequestHeaders(),
            body = getSponsoredStoriesRequestBody(),
            conservative = true,
        )
        return client.fetchBodyOrNull(request)
    }

    private fun getSponsoredStoriesRequestHeaders(): MutableHeaders {
        return if (config.userAgent.isNullOrEmpty()) {
            MutableHeaders(
                CONTENT_TYPE to "$CONTENT_TYPE_APPLICATION_JSON; charset=UTF-8",
            )
        } else {
            MutableHeaders(
                CONTENT_TYPE to "$CONTENT_TYPE_APPLICATION_JSON; charset=UTF-8",
                USER_AGENT to config.userAgent,
            )
        }
    }

    private fun getSponsoredStoriesRequestBody(): Body {
        val params = mapOf(
            REQUEST_BODY_CONTEXT_ID_KEY to config.contextId,
            REQUEST_BODY_PLACEMENTS_KEY to config.placements.map {
                mapOf(
                    REQUEST_BODY_PLACEMENT_KEY to it.placement,
                    REQUEST_BODY_COUNT_KEY to it.count,
                )
            },
        )

        return Body(JSONObject(params).toString().byteInputStream())
    }

    /**
     * Request to delete any data persisted associated with the [config.contextId].
     *
     * @return true if the delete operation was successful and false otherwise.
     */
    @WorkerThread
    fun deleteUser(): Boolean {
        val url = Uri.Builder()
            .encodedPath(MARS_ENDPOINT_BASE_URL + MARS_ENDPOINT_DELETE_USER_PATH)
            .build()
            .toString()
        val request = Request(
            url = url,
            method = Method.DELETE,
            headers = MutableHeaders(
                CONTENT_TYPE to "$CONTENT_TYPE_APPLICATION_JSON; charset=UTF-8",
                "Accept" to "*/*",
            ),
            body = getDeleteUserRequestBody(),
            conservative = true,
        )

        val response: Response? = try {
            client.fetch(request)
        } catch (e: IOException) {
            logger.debug("Network error", e)
            null
        }

        response?.close()
        return response?.isSuccess ?: false
    }

    private fun getDeleteUserRequestBody(): Body {
        val params = mapOf(
            REQUEST_BODY_CONTEXT_ID_KEY to config.contextId,
        )

        return Body(JSONObject(params).toString().byteInputStream())
    }

    companion object {
        /**
         * Returns a new instance of [MarsSpocsEndpointRaw].
         *
         * @param client The HTTP [Client] to use for network requests.
         * @param config Configuration for the sponsored stories request.
         */
        fun newInstance(
            client: Client,
            config: MarsSpocsRequestConfig,
        ) = MarsSpocsEndpointRaw(client, config)

        /**
         * Convenience variable for checking whether the current build is a debug build and
         * overwriting for tests.
         */
        @VisibleForTesting
        internal var isDebugBuild = BuildConfig.DEBUG

        /**
         * Returns the MARS endpoint base URL for fetching sponsored content given whether or not
         * this is a development or production build environment.
         */
        @VisibleForTesting
        internal val baseUrl
            get() = if (isDebugBuild) {
                MARS_ENDPOINT_STAGING_BASE_URL
            } else {
                MARS_ENDPOINT_BASE_URL
            }
    }
}
