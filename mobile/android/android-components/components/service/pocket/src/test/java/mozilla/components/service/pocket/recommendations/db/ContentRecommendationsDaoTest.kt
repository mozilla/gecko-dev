/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.db

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@RunWith(AndroidJUnit4::class)
class ContentRecommendationsDaoTest {

    private lateinit var database: ContentRecommendationsDatabase
    private lateinit var dao: ContentRecommendationsDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()
        database = Room
            .inMemoryDatabaseBuilder(testContext, ContentRecommendationsDatabase::class.java)
            .allowMainThreadQueries()
            .build()
        dao = database.contentRecommendationsDao()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun `WHEN a content recommendation is inserted and fetched THEN the newly inserted recommendation is returned`() = runTest {
        val recommendation = PocketTestResources.contentRecommendationEntity
        dao.insert(recommendations = listOf(recommendation))

        val recommendations = dao.getContentRecommendations()

        assertEquals(listOf(recommendation), recommendations)
    }

    @Test
    fun `WHEN a content recommendation is deleted THEN content recommendation is removed from the database`() = runTest {
        val recommendation1 = PocketTestResources.contentRecommendationEntity
        val recommendation2 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 2,
        )

        dao.insert(recommendations = listOf(recommendation1, recommendation2))
        dao.delete(recommendations = listOf(recommendation1))

        val recommendations = dao.getContentRecommendations()

        assertEquals(listOf(recommendation2), recommendations)
    }

    @Test
    fun `WHEN a content recommendation is updated and fetched THEN the updated recommendation is returned`() = runTest {
        val recommendation1 = PocketTestResources.contentRecommendationEntity
        val recommendation2 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 2,
        )

        dao.insert(recommendations = listOf(recommendation1, recommendation2))

        val updatedRecommendation1 = recommendation1.copy(
            title = "title",
            isTimeSensitive = false,
        )
        dao.update(recommendations = listOf(updatedRecommendation1))

        val recommendations = dao.getContentRecommendations()

        assertEquals(listOf(updatedRecommendation1, recommendation2), recommendations)
    }

    @Test
    fun `WHEN content recommendations impressions are updated THEN content recommendations impressions are updated in the database`() = runTest {
        val recommendation1 = PocketTestResources.contentRecommendationEntity
        val recommendation2 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 2,
            impressions = recommendation1.impressions * 2,
        )
        val recommendation3 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 3,
            impressions = recommendation1.impressions * 3,
        )
        val recommendation4 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 4,
            impressions = recommendation1.impressions * 4,
        )

        dao.insert(
            recommendations = listOf(
                recommendation1,
                recommendation2,
                recommendation3,
                recommendation4,
            ),
        )

        val updatedRecommendationImpression1 = ContentRecommendationImpression(
            corpusItemId = recommendation1.corpusItemId,
            impressions = 10,
        )
        val updatedRecommendationImpression3 = ContentRecommendationImpression(
            corpusItemId = recommendation3.corpusItemId,
            impressions = 11,
        )

        dao.updateRecommendationsImpressions(
            recommendations = listOf(
                updatedRecommendationImpression1,
                updatedRecommendationImpression3,
            ),
        )

        val recommendations = dao.getContentRecommendations()

        assertEquals(
            listOf(
                recommendation1.copy(impressions = updatedRecommendationImpression1.impressions),
                recommendation2,
                recommendation3.copy(impressions = updatedRecommendationImpression3.impressions),
                recommendation4,
            ),
            recommendations,
        )
    }

    @Test
    fun `GIVEN an updated content recommendation impression is provided that is not in the storage WHEN content recommendations impressions are updated THEN no update occurs`() = runTest {
        val recommendation1 = PocketTestResources.contentRecommendationEntity
        val recommendation2 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 2,
            impressions = recommendation1.impressions * 2,
        )
        val recommendation3 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 3,
            impressions = recommendation1.impressions * 3,
        )
        val recommendation4 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 4,
            impressions = recommendation1.impressions * 4,
        )

        dao.insert(
            recommendations = listOf(
                recommendation1,
                recommendation2,
                recommendation3,
                recommendation4,
            ),
        )

        val updatedRecommendationImpression1 = ContentRecommendationImpression(
            corpusItemId = "corpusItemId",
            impressions = 10,
        )

        dao.updateRecommendationsImpressions(
            recommendations = listOf(updatedRecommendationImpression1),
        )

        val recommendations = dao.getContentRecommendations()

        assertEquals(
            listOf(
                recommendation1,
                recommendation2,
                recommendation3,
                recommendation4,
            ),
            recommendations,
        )
    }

    @Test
    fun `GIVEN a new list of content recommendations WHEN the database is cleaned and updated THEN the new list of recommendation is stored`() = runTest {
        setupDatabaseForTransactions()

        val recommendation1 = PocketTestResources.contentRecommendationEntity
        val recommendation2 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 2,
        )
        val recommendation3 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 3,
        )

        dao.insert(recommendations = listOf(recommendation1, recommendation2))

        val newRecommendations = listOf(recommendation3)
        dao.cleanAndUpdateContentRecommendations(recommendations = newRecommendations)

        val recommendations = dao.getContentRecommendations()

        assertEquals(newRecommendations, recommendations)
    }

    @Test
    fun `GIVEN an update to existing content recommendations WHEN the database is cleaned and updated THEN the updated recommendation is stored`() = runTest {
        setupDatabaseForTransactions()

        val recommendation1 = PocketTestResources.contentRecommendationEntity
        val recommendation2 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 2,
        )
        val recommendation3 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 3,
        )

        dao.insert(recommendations = listOf(recommendation1, recommendation2, recommendation3))

        val updatedRecommendation2 = recommendation2.copy(
            title = "title",
            receivedRank = 10,
        )
        val newRecommendations = listOf(
            recommendation1,
            updatedRecommendation2,
            recommendation3,
        )
        dao.cleanAndUpdateContentRecommendations(recommendations = newRecommendations)

        val recommendations = dao.getContentRecommendations()

        assertEquals(newRecommendations, recommendations)
    }

    @Test
    fun `GIVEN new recommendations to the existing content recommendations WHEN the database is cleaned and updated THEN the new recommendations are stored`() = runTest {
        setupDatabaseForTransactions()

        val recommendation1 = PocketTestResources.contentRecommendationEntity
        val recommendation2 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 2,
        )
        val recommendation3 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 3,
        )
        val recommendation4 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 4,
        )
        val recommendation5 = PocketTestResources.contentRecommendationEntity.copy(
            corpusItemId = recommendation1.corpusItemId + 5,
        )

        dao.insert(recommendations = listOf(recommendation1, recommendation2, recommendation3))

        val updatedRecommendation2 = recommendation2.copy(
            title = "title",
            receivedRank = 10,
        )
        val newRecommendations = listOf(
            recommendation1,
            updatedRecommendation2,
            recommendation4,
            recommendation5,
        )
        dao.cleanAndUpdateContentRecommendations(recommendations = newRecommendations)

        val recommendations = dao.getContentRecommendations()

        assertEquals(newRecommendations, recommendations)
    }

    /**
     * Sets an executor to be used for database transactions.
     * Needs to be used along with "runTest" to ensure waiting for transactions to finish but not hang tests.
     */
    private fun setupDatabaseForTransactions() {
        database = Room
            .inMemoryDatabaseBuilder(testContext, ContentRecommendationsDatabase::class.java)
            .setTransactionExecutor(executor)
            .allowMainThreadQueries()
            .build()
        dao = database.contentRecommendationsDao()
    }
}
