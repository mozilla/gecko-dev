/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.db

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction
import androidx.room.Update
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase.Companion.CONTENT_RECOMMENDATIONS_TABLE

/**
 * Internal DAO for accessing [ContentRecommendationEntity] instances.
 */
@Dao
internal interface ContentRecommendationsDao {
    @Query("SELECT * FROM $CONTENT_RECOMMENDATIONS_TABLE")
    suspend fun getContentRecommendations(): List<ContentRecommendationEntity>

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insert(recommendations: List<ContentRecommendationEntity>)

    @Delete
    suspend fun delete(recommendations: List<ContentRecommendationEntity>)

    @Update
    suspend fun update(recommendations: List<ContentRecommendationEntity>)

    @Update(entity = ContentRecommendationEntity::class)
    suspend fun updateRecommendationsImpressions(recommendations: List<ContentRecommendationImpression>)

    /**
     * Replaces the existing content recommendations in the database with the provided
     * [recommendations]. This will remove any existing recommendations that are no longer
     * part of the provided [recommendations], updating the metadata in existing recommendations
     * that are still relevant, and persisting any new recommendations in storage.
     *
     * @param recommendations The new list of [ContentRecommendationEntity] to persist in storage.
     */
    @Transaction
    suspend fun cleanAndUpdateContentRecommendations(recommendations: List<ContentRecommendationEntity>) {
        val oldRecommendations = getContentRecommendations()
        val oldCorpusItemIds = oldRecommendations.map { it.corpusItemId }
        val newCorpusItemIds = recommendations.map { it.corpusItemId }

        val existingRecommendationsToDelete =
            oldRecommendations.filterNot { newCorpusItemIds.contains(it.corpusItemId) }
        delete(existingRecommendationsToDelete)

        val existingRecommendationsToUpdate =
            recommendations.filter { oldCorpusItemIds.contains(it.corpusItemId) }
        update(existingRecommendationsToUpdate)

        val newRecommendationsToInsert =
            recommendations.filterNot { oldCorpusItemIds.contains(it.corpusItemId) }
        insert(newRecommendationsToInsert)
    }
}
