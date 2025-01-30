/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketStory.SponsoredContent
import mozilla.components.service.pocket.mars.api.MarsSpocsEndpoint
import mozilla.components.service.pocket.mars.api.MarsSpocsRequestConfig
import mozilla.components.service.pocket.stories.api.PocketResponse.Success

/**
 * Use cases for sponsored contents actions.
 *
 * @param appContext Application [Context]. Prefer sending application context to limit the
 * possibility of even small leaks.
 * @param client The HTTP [Client] to use for network requests.
 * @param config Configuration for sponsored contents request.
 */
internal class SponsoredContentsUseCases(
    private val appContext: Context,
    private val client: Client,
    private val config: MarsSpocsRequestConfig,
) {

    /**
     * Fetches the latest sponsored contents from the provider and store them in storage.
     */
    internal val refreshSponsoredContents by lazy {
        RefreshSponsoredContents(appContext, client, config)
    }

    /**
     * Get the list of sponsored contents.
     */
    internal val getSponsoredContents by lazy {
        GetSponsoredContents(appContext)
    }

    /**
     * Records the sponsored contents that has been viewed (impression).
     */
    internal val recordImpressions by lazy {
        RecordImpressions(appContext)
    }

    /**
     * Use case for fetching and refreshing the list of sponsored contents in storage.
     *
     * @param context Application [Context]. Prefer sending application context to limit the
     * possibility of even small leaks.
     * @param client The HTTP [Client] to use for network requests.
     *
     */
    internal inner class RefreshSponsoredContents(
        @get:VisibleForTesting
        internal val context: Context = this@SponsoredContentsUseCases.appContext,
        @get:VisibleForTesting
        internal val client: Client = this@SponsoredContentsUseCases.client,
        @get:VisibleForTesting
        internal val config: MarsSpocsRequestConfig = this@SponsoredContentsUseCases.config,
    ) {

        /**
         * Fetches sponsored content based on the provided [config] and stores the items in
         * storage.
         *
         * @return True if the operation was successful and false otherwise.
         */
        suspend operator fun invoke(): Boolean {
            val response = getSponsoredContentsProvider(client, config).getSponsoredStories()

            if (response !is Success) {
                return false
            }

            getSponsoredContentsRepository(context).addSponsoredContents(response.data)

            return true
        }
    }

    /**
     * Use case for getting the list of available sponsored contents from storage.
     *
     * @param context Application [Context]. Prefer sending application context to limit the
     * possibility of even small leaks.
     */
    internal inner class GetSponsoredContents(
        @get:VisibleForTesting
        internal val context: Context = this@SponsoredContentsUseCases.appContext,
    ) {

        /**
         * Returns the list of [SponsoredContent]s from storage.
         */
        suspend operator fun invoke(): List<SponsoredContent> {
            return getSponsoredContentsRepository(context).getAllSponsoredContent()
        }
    }

    /**
     * Use case for recording sponsored content impressions.
     *
     * @param context Application [Context]. Prefer sending application context to limit the
     * possibility of even small leaks.
     */
    internal inner class RecordImpressions(
        @get:VisibleForTesting
        internal val context: Context = this@SponsoredContentsUseCases.appContext,
    ) {

        /**
         * Records the sponsored content impressions from the provided list of sponsored content
         * URLs.
         *
         * @param impressions A list of sponsored content URLs that have been viewed.
         */
        suspend operator fun invoke(impressions: List<String>) {
            if (impressions.isEmpty()) return

            getSponsoredContentsRepository(context).recordImpressions(impressions)
        }
    }

    @VisibleForTesting
    internal fun getSponsoredContentsRepository(context: Context) =
        SponsoredContentsRepository(context)

    @VisibleForTesting
    internal fun getSponsoredContentsProvider(
        client: Client,
        config: MarsSpocsRequestConfig,
    ) = MarsSpocsEndpoint.newInstance(
        client = client,
        config = config,
    )
}
