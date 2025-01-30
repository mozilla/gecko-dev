/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.db

import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase

/**
 * Internal entity representing a sponsored content.
 *
 * @property url The url of the sponsored content.
 * @property title The title of the sponsored content.
 * @property clickUrl URL to be called when the sponsored content is clicked.
 * @property impressionUrl URL to be called when the sponsored content is viewed (impression).
 * @property imageUrl The image URL of the sponsored content.
 * @property domain The domain of where the sponsored content is hosted.
 * @property excerpt  A short excerpt from the sponsored content.
 * @property sponsor The name of the sponsor.
 * @property blockKey The block key generated from encoding the advertiser name and ad placement.
 * @property flightCapCount Indicates how many times a sponsored content can be shown within a
 * [flightCapPeriod].
 * @property flightCapPeriod Indicates the period (number of seconds) in which at most
 * [flightCapCount] sponsored content can be shown.
 * @property priority The priority of the sponsored content in the ranking.
 */
@Entity(tableName = ContentRecommendationsDatabase.SPONSORED_CONTENT_TABLE)
internal data class SponsoredContentEntity(
    @PrimaryKey
    val url: String,
    val title: String,
    val clickUrl: String,
    val impressionUrl: String,
    val imageUrl: String,
    val domain: String,
    val excerpt: String,
    val sponsor: String,
    val blockKey: String,
    val flightCapCount: Int,
    val flightCapPeriod: Int,
    val priority: Int,
)
