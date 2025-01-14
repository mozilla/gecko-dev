/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.api

import kotlinx.serialization.Serializable

/**
 * The content recommendation payload response.
 *
 * This should follow the response schema in
 * https://merino.services.mozilla.com/docs#/default/curated_content_api_v1_curated_recommendations_post.
 *
 * @property recommendedAt A timestamp indicating when the content recommendations was recommended.
 * @property data A list of [ContentRecommendationResponseItem] from the response payload.
 */
@Serializable
internal data class ContentRecommendationsResponse(
    val recommendedAt: Long,
    val data: List<ContentRecommendationResponseItem>,
)

/**
 * A content recommendation item in the payload response.
 *
 * This should follow the items schema in
 * https://merino.services.mozilla.com/docs#/default/curated_content_api_v1_curated_recommendations_post.
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
 */
@Serializable
internal data class ContentRecommendationResponseItem(
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
)
