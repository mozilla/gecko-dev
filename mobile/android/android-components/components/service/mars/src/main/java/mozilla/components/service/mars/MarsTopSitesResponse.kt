/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.mars

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import mozilla.components.feature.top.sites.TopSite

const val NEW_TAB_TILE_1_PLACEMENT_KEY = "newtab_mobile_tile_1"
const val NEW_TAB_TILE_2_PLACEMENT_KEY = "newtab_mobile_tile_2"

/**
 * The top sites payload response from the MARS API.
 *
 * @property tile1 A list of [MarsTopSiteResponseItem] for the first top site tile.
 * @property tile2 A list of [MarsTopSiteResponseItem] for the second top site tile.
 */
@Serializable
internal data class MarsTopSitesResponse(
    @SerialName(NEW_TAB_TILE_1_PLACEMENT_KEY) val tile1: List<MarsTopSiteResponseItem>,
    @SerialName(NEW_TAB_TILE_2_PLACEMENT_KEY) val tile2: List<MarsTopSiteResponseItem>,
)

/**
 * A top site item in the payload response.
 */
@Serializable
internal data class MarsTopSiteResponseItem(
    val name: String,
    val url: String,
    @SerialName("image_url") val imageUrl: String,
    val callbacks: MarsTopSiteResponseCallbacks,
    val format: String,
    @SerialName("block_key") val blockKey: String,
)

/**
 * An object containing the callback URLs for click and impression of a top site tile.
 */
@Serializable
internal data class MarsTopSiteResponseCallbacks(
    @SerialName("click") val clickUrl: String,
    @SerialName("impression") val impressionUrl: String,
)

/**
 * Flatten and return a list of [TopSite.Provided] from [MarsTopSitesResponse].
 */
internal fun MarsTopSitesResponse.getTopSites(): List<TopSite.Provided> =
    tile1.map { it.toTopSite() } + tile2.map { it.toTopSite() }

private fun MarsTopSiteResponseItem.toTopSite() = TopSite.Provided(
    id = null,
    title = name,
    url = url,
    clickUrl = callbacks.clickUrl,
    imageUrl = imageUrl,
    impressionUrl = callbacks.impressionUrl,
    createdAt = null,
)
