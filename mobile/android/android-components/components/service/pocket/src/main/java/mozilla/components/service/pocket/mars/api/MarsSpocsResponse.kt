/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.api

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

const val NEW_TAB_SPOCS_PLACEMENT_KEY = "newtab_mobile_spocs"

/**
 * The sponsored content payload response.
 *
 * @property spocs The list of [MarsSpocsResponseItem]s from the response payload.
 */
@Serializable
internal data class MarsSpocsResponse(
    @SerialName(NEW_TAB_SPOCS_PLACEMENT_KEY) val spocs: List<MarsSpocsResponseItem>,
)

/**
 * A sponsored content payload response item.
 *
 * @property format The format type of the sponsored content.
 * @property url The url of the sponsored content.
 * @property callbacks The [MarsSpocResponseCallbacks] object containing callback URLs for click
 * and impression tracking.
 * @property imageUrl The image URL of the sponsored content.
 * @property title The title of the sponsored content.
 * @property domain The domain of where the sponsored content is hosted.
 * @property excerpt A short excerpt from the sponsored content.
 * @property sponsor The name of the sponsor.
 * @property blockKey The block key generated from encoding the advertiser name and ad placement.
 * @property caps Frequency capping information for the sponsored content.
 * @property ranking Ranking information for personalized content.
 */
@Serializable
internal data class MarsSpocsResponseItem(
    val format: String,
    val url: String,
    val callbacks: MarsSpocResponseCallbacks,
    @SerialName("image_url") val imageUrl: String,
    val title: String,
    val domain: String,
    val excerpt: String,
    val sponsor: String,
    @SerialName("block_key") val blockKey: String,
    val caps: MarsSpocFrequencyCaps,
    val ranking: MarsSpocRanking,
)

/**
 * Sponsored content callback URLs for click and impression tracking.
 *
 * @property clickUrl URL to be called when the sponsored content is clicked.
 * @property impressionUrl URL to be called when the sponsored content is viewed (impression).
 */
@Serializable
internal data class MarsSpocResponseCallbacks(
    @SerialName("click") val clickUrl: String,
    @SerialName("impression") val impressionUrl: String,
)

/**
 * Sponsored content frequency capping information.
 *
 * @property capKey A key that identifies the frequency cap.
 * @property day Number of times to show the same ad during a one day period.
 */
@Serializable
internal data class MarsSpocFrequencyCaps(
    @SerialName("cap_key") val capKey: String,
    val day: Int,
)

/**
 * Sponsored content ranking information for personalized content.
 *
 * @property priority The priority of the sponsored content in the ranking.
 * @property itemScore The overall score of the item.
 */
@Serializable
internal data class MarsSpocRanking(
    val priority: Int,
    @SerialName("item_score") val itemScore: Float,
)
