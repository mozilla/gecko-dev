/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketStory.ContentRecommendation
import mozilla.components.service.pocket.ext.toContentRecommendation
import mozilla.components.service.pocket.ext.toContentRecommendationEntity
import mozilla.components.service.pocket.ext.toImpressions
import mozilla.components.service.pocket.recommendations.api.ContentRecommendationsResponse
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase

/**
 * A storage wrapper for handling CRUD operations over the content recommendation storage.
 */
internal class ContentRecommendationsRepository(context: Context) {
    private val database: Lazy<ContentRecommendationsDatabase> = lazy {
        ContentRecommendationsDatabase.get(context)
    }

    @VisibleForTesting
    internal val contentRecommendationsDao by lazy { database.value.contentRecommendationsDao() }

    /**
     * Returns all the [ContentRecommendation]s that are persisted in storage.
     *
     * @return the list of [ContentRecommendation]s that is stored in the content recommendation
     * storage.
     */
    suspend fun getContentRecommendations(): List<ContentRecommendation> {
        return contentRecommendationsDao.getContentRecommendations()
            .map { it.toContentRecommendation() }
    }

    /**
     * Updates the number of impressions (times shown) for the provided list of
     * [ContentRecommendation]s that are persisted in storage.
     *
     * @param recommendationsShown The list of [ContentRecommendation]s to update.
     */
    suspend fun updateContentRecommendationsImpressions(
        recommendationsShown: List<ContentRecommendation>,
    ) {
        return contentRecommendationsDao.updateRecommendationsImpressions(
            recommendationsShown.map { it.toImpressions() },
        )
    }

    /**
     * Adds the provided [ContentRecommendationsResponse] to storage updating and
     * replacing the content recommendations in storage.
     *
     * @param response The new [ContentRecommendationsResponse] to store in storage.
     */
    suspend fun updateContentRecommendations(response: ContentRecommendationsResponse) {
        contentRecommendationsDao.cleanAndUpdateContentRecommendations(
            response.data.map { it.toContentRecommendationEntity(response.recommendedAt) },
        )
    }
}
