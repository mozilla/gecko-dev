/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.db

import androidx.room.Entity
import androidx.room.PrimaryKey

/**
 * Internal entity represent a content recommendation.
 *
 * @property corpusItemId A content identifier that corresponds uniquely to the URL.
 * @property scheduledCorpusItemId The ID of the scheduled corpus item for this recommendation.
 * @property url The url of the recommendation.
 * @property title The title of the recommendation.
 * @property excerpt An excerpt of the recommendation.
 * @property topic The topic of interest under which similar recommendations are grouped.
 * @property publisher The publisher of the recommendation.
 * @property isTimeSensitive Whether or not the recommendation is time sensitive.
 * @property imageUrl The image URL of the recommendation.
 * @property tileId The tile ID of the recommendation.
 * @property receivedRank The original position/sort order of this item. This is provided to
 * include in telemetry payloads.
 * @property recommendedAt A timestamp indicating when the content recommendation was recommended.
 * @property impressions The number of impressions (times shown) of the recommendation.
 */
@Entity(tableName = ContentRecommendationsDatabase.CONTENT_RECOMMENDATIONS_TABLE)
internal data class ContentRecommendationEntity(
    @PrimaryKey
    val corpusItemId: String,
    val scheduledCorpusItemId: String,
    val url: String,
    val title: String,
    val excerpt: String,
    val topic: String?,
    val publisher: String,
    val isTimeSensitive: Boolean,
    val imageUrl: String,
    val tileId: Long,
    val receivedRank: Int,
    val recommendedAt: Long,
    val impressions: Long,
)

/**
 * A [ContentRecommendationEntity] containing only the [impressions] metadata for allowing quick
 * updates.
 *
 * @property corpusItemId A content identifier that corresponds uniquely to the URL.
 * @property impressions The number of impressions (times shown) of the recommendation.
 */
internal data class ContentRecommendationImpression(
    val corpusItemId: String,
    val impressions: Long,
)
