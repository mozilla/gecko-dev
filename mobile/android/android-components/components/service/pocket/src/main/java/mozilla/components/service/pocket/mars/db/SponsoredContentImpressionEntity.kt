/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.db

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase

/**
 * Internal entity representing a sponsored content impression.
 *
 * @property url The url of the sponsored content.
 * @property impressionId Unique id of the impression.
 * @property impressionDateInSeconds Epoch based timestamp expressed in seconds
 * (from System.currentTimeMillis / 1000) for when the sponsored content identified by [url] was
 * shown to the user.
 */
@Entity(
    tableName = ContentRecommendationsDatabase.SPONSORED_CONTENT_IMPRESSION_TABLE,
    foreignKeys = [
        ForeignKey(
            entity = SponsoredContentEntity::class,
            parentColumns = arrayOf("url"),
            childColumns = arrayOf("url"),
            onDelete = ForeignKey.CASCADE,
        ),
    ],
    indices = [
        Index(value = ["url"], unique = false),
    ],
)
internal data class SponsoredContentImpressionEntity(
    val url: String,
) {
    @PrimaryKey(autoGenerate = true)
    var impressionId: Int = 0
    var impressionDateInSeconds: Long = System.currentTimeMillis() / 1000
}
