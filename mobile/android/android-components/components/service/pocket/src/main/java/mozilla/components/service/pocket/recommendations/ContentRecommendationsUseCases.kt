/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.ContentRecommendationsRequestConfig
import mozilla.components.service.pocket.PocketStory.ContentRecommendation
import mozilla.components.service.pocket.recommendations.api.ContentRecommendationsEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse

/**
 * Possible actions regarding the list of content recommendations.
 *
 * @param appContext Application [Context]. Prefer sending application context to limit the
 * possibility of even small leaks.
 * @param client The HTTP [Client] to use for network requests.
 * @param config Configuration for content recommendations request.
 */
internal class ContentRecommendationsUseCases(
    private val appContext: Context,
    private val client: Client,
    private val config: ContentRecommendationsRequestConfig,
) {

    /**
     * Get the list of available content recommendations.
     */
    internal val getContentRecommendations by lazy { GetContentRecommendations(appContext) }

    /**
     * Fetches the list of content recommendations and stores it in storage.
     */
    internal val fetchContentRecommendations by lazy {
        FetchContentRecommendations(appContext, client)
    }

    /**
     * Updates the number of impressions (times shown) for a list of content recommendations.
     */
    internal val updateRecommendationsImpressions by lazy {
        UpdateRecommendationsImpressions(appContext)
    }

    /**
     * Use case for getting the list of available content recommendations from storage.
     *
     * @param context Application [Context]. Prefer sending application context to limit the
     * possibility of even small leaks.
     */
    internal inner class GetContentRecommendations(
        @get:VisibleForTesting
        internal val context: Context = this@ContentRecommendationsUseCases.appContext,
    ) {

        /**
         * Returns the list of [ContentRecommendation]s from storage.
         */
        suspend operator fun invoke(): List<ContentRecommendation> {
            return getContentRecommendationsRepository(context).getContentRecommendations()
        }
    }

    /**
     * Use case for fetching and refreshing the list of content recommendations in storage.
     *
     * @param context Application [Context]. Prefer sending application context to limit the
     * possibility of even small leaks.
     * @param client The HTTP [Client] to use for network requests.
     * @param config Configuration for content recommendations request.
     */
    internal inner class FetchContentRecommendations(
        @get:VisibleForTesting
        internal val context: Context = this@ContentRecommendationsUseCases.appContext,
        @get:VisibleForTesting
        internal val client: Client = this@ContentRecommendationsUseCases.client,
        @get:VisibleForTesting
        internal val config: ContentRecommendationsRequestConfig = this@ContentRecommendationsUseCases.config,
    ) {

        /**
         * Fetches content recommendations based on the provided [config] and stores the items
         * in storage.
         */
        suspend operator fun invoke(): Boolean {
            val response =
                getContentRecommendationsEndpoint(client, config).getContentRecommendations()

            if (response !is PocketResponse.Success) {
                return false
            }

            getContentRecommendationsRepository(context)
                .updateContentRecommendations(response.data)

            return true
        }
    }

    /**
     * Use case for updating the number of impressions (times shown) for a list of content
     * recommendations.
     *
     * @param context Application [Context]. Prefer sending application context to limit the
     * possibility of even small leaks.
     */
    internal inner class UpdateRecommendationsImpressions(
        @get:VisibleForTesting
        internal val context: Context = this@ContentRecommendationsUseCases.appContext,
    ) {

        /**
         * Updates the number of impressions (times shown) for the provided list of
         * [ContentRecommendation]s.
         *
         * @param recommendationsShown The list of [ContentRecommendation]s that have an updated
         * impressions.
         */
        suspend operator fun invoke(recommendationsShown: List<ContentRecommendation>) {
            if (recommendationsShown.isEmpty()) return

            getContentRecommendationsRepository(context)
                .updateContentRecommendationsImpressions(recommendationsShown)
        }
    }

    @VisibleForTesting
    internal fun getContentRecommendationsRepository(context: Context) =
        ContentRecommendationsRepository(context)

    @VisibleForTesting
    internal fun getContentRecommendationsEndpoint(
        client: Client,
        config: ContentRecommendationsRequestConfig,
    ) = ContentRecommendationsEndpoint.newInstance(client, config)
}
