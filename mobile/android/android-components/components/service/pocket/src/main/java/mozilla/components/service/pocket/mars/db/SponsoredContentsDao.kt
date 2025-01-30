/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.db

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase
import java.util.concurrent.TimeUnit

/**
 * Internal DAO for accessing [SponsoredContentEntity] and [SponsoredContentImpressionEntity]
 * instances.
 */
@Dao
internal interface SponsoredContentsDao {
    @Transaction
    suspend fun cleanOldAndInsertNewSponsoredContents(sponsoredContents: List<SponsoredContentEntity>) {
        val newSponsoredContents = sponsoredContents.map { it.url }
        val oldSponsoredContentsToDelete = getSponsoredContents()
            .filterNot { newSponsoredContents.contains(it.url) }

        deleteSponsoredContents(oldSponsoredContentsToDelete)
        insertSponsoredContents(sponsoredContents)
    }

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertSponsoredContents(sponsoredContents: List<SponsoredContentEntity>)

    @Transaction
    suspend fun recordImpressions(impressions: List<SponsoredContentImpressionEntity>) {
        impressions.forEach {
            recordImpression(it.url, it.impressionDateInSeconds)
        }
    }

    /**
     * INSERT OR IGNORE method needed to prevent against "FOREIGN KEY constraint failed" exceptions
     * if clients try to insert new sponsored content impressions not existing anymore in the
     * database in cases where a different list of sponsored contents were downloaded but the
     * client operates with stale in-memory data.
     *
     * @param targetUrl The url of the sponsored content.
     * @param targetImpressionDateInSeconds The timestamp expressed in seconds from Epoch for this
     * impression. Defaults to the current time expressed in seconds as get from
     * `System.currentTimeMillis / 1000`.
     */
    @Suppress("MaxLineLength")
    @Query(
        "WITH newImpression(url, impressionDateInSeconds) AS (VALUES" +
            "(:targetUrl, :targetImpressionDateInSeconds)" +
            ")" +
            "INSERT INTO ${ContentRecommendationsDatabase.SPONSORED_CONTENT_IMPRESSION_TABLE}(url, impressionDateInSeconds) " +
            "SELECT impression.url, impression.impressionDateInSeconds " +
            "FROM newImpression impression " +
            "WHERE EXISTS (SELECT 1 FROM ${ContentRecommendationsDatabase.SPONSORED_CONTENT_TABLE} spoc WHERE spoc.url = impression.url)",
    )
    suspend fun recordImpression(
        targetUrl: String,
        targetImpressionDateInSeconds: Long = TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
    )

    @Query("DELETE FROM ${ContentRecommendationsDatabase.SPONSORED_CONTENT_TABLE}")
    suspend fun deleteAllSponsoredContents()

    @Delete
    suspend fun deleteSponsoredContents(sponsoredContents: List<SponsoredContentEntity>)

    @Query("SELECT * FROM ${ContentRecommendationsDatabase.SPONSORED_CONTENT_TABLE}")
    suspend fun getSponsoredContents(): List<SponsoredContentEntity>

    @Query("SELECT * FROM ${ContentRecommendationsDatabase.SPONSORED_CONTENT_IMPRESSION_TABLE}")
    suspend fun getSponsoredContentImpressions(): List<SponsoredContentImpressionEntity>
}
