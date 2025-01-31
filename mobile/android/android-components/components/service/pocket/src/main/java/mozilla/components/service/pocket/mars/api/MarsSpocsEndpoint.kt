/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.api

import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.mars.api.MarsSpocsEndpoint.Companion.newInstance
import mozilla.components.service.pocket.stories.api.PocketResponse

/**
 * Performs requests to retrieve the sponsored stories from the provided [rawEndpoint].
 *
 * @see [newInstance] to retrieve an instance.
 */
internal class MarsSpocsEndpoint internal constructor(
    @get:VisibleForTesting internal val rawEndpoint: MarsSpocsEndpointRaw,
) {
    /**
     * Returns a response containing the sponsored stories from the provided endpoint on success.
     *
     * @return a [PocketResponse.Success] with the decoded payload response of the sponsored
     * stories or a [PocketResponse.Failure] on error.
     */
    @WorkerThread
    fun getSponsoredStories(): PocketResponse<MarsSpocsResponse> {
        val response = rawEndpoint.getSponsoredStories()?.let {
            try {
                val json = Json { ignoreUnknownKeys = true }
                json.decodeFromString<MarsSpocsResponse>(it)
            } catch (e: SerializationException) {
                null
            }
        }

        return PocketResponse.wrap(response)
    }

    /**
     * Request to delete any data persisted associated with the user.
     *
     * @return true if the delete operation was successful and false otherwise.
     */
    @WorkerThread
    fun deleteUser(): PocketResponse<Boolean> {
        val response = rawEndpoint.deleteUser()
        return PocketResponse.wrap(response)
    }

    companion object {
        /**
         * Returns a new instance of [MarsSpocsEndpoint].
         *
         * @param client The HTTP [Client] to use for network requests.
         * @param config Configuration for the sponsored stories request.
         */
        fun newInstance(
            client: Client,
            config: MarsSpocsRequestConfig,
        ): MarsSpocsEndpoint {
            val rawEndpoint = MarsSpocsEndpointRaw.newInstance(client, config)
            return MarsSpocsEndpoint(rawEndpoint)
        }
    }
}
