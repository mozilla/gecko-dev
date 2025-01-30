/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketStory.SponsoredContent
import mozilla.components.service.pocket.ext.toSponsoredContent
import mozilla.components.service.pocket.ext.toSponsoredContentEntity
import mozilla.components.service.pocket.mars.api.MarsSpocsResponse
import mozilla.components.service.pocket.mars.db.SponsoredContentImpressionEntity
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase

/**
 * A storage wrapper for handling CRUD operations for sponsored content.
 */
internal class SponsoredContentsRepository(context: Context) {
    private val database: Lazy<ContentRecommendationsDatabase> = lazy {
        ContentRecommendationsDatabase.get(context)
    }

    @VisibleForTesting
    internal val dao by lazy { database.value.sponsoredContentsDao() }

    /**
     * Returns all the [SponsoredContent]s that are persisted in storage.
     *
     * @return the list of [SponsoredContent]s in storage.
     */
    suspend fun getAllSponsoredContent(): List<SponsoredContent> {
        val sponsoredContents = dao.getSponsoredContents()
        val impressions = dao.getSponsoredContentImpressions().groupBy { it.url }

        return sponsoredContents.map { entity ->
            entity.toSponsoredContent(
                impressions[entity.url]
                    ?.map { impression -> impression.impressionDateInSeconds }
                    ?: emptyList(),
            )
        }
    }

    /**
     * Deletes all the sponsored contents that are persisted in storage.
     */
    suspend fun deleteAllSponsoredContents() {
        dao.deleteAllSponsoredContents()
    }

    /**
     * Adds the provided [MarsSpocsResponse] to storage removing any sponsored contents that
     * are no longer part of the new response and inserting the new sponsored contents to storage.
     *
     * @param response The new [MarsSpocsResponse] to store in storage.
     */
    suspend fun addSponsoredContents(response: MarsSpocsResponse) {
        dao.cleanOldAndInsertNewSponsoredContents(response.spocs.map { it.toSponsoredContentEntity() })
    }

    /**
     * Records new impression records for sponsored contents that have been viewed.
     *
     * @param impressions List containing the URLs of sponsored contents that have been view.
     */
    suspend fun recordImpressions(impressions: List<String>) {
        dao.recordImpressions(impressions.map { SponsoredContentImpressionEntity(it) })
    }
}
