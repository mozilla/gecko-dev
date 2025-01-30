/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.db

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@RunWith(AndroidJUnit4::class)
class SponsoredContentsDaoTest {

    private lateinit var database: ContentRecommendationsDatabase
    private lateinit var dao: SponsoredContentsDao
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
        dao = database.sponsoredContentsDao()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun `WHEN a sponsored content is inserted and fetched THEN the newly inserted sponsored content is returned`() = runTest {
        val sponsoredContent = PocketTestResources.sponsoredContentEntity
        dao.insertSponsoredContents(sponsoredContents = listOf(sponsoredContent))

        val sponsoredContents = dao.getSponsoredContents()

        assertEquals(listOf(sponsoredContent), sponsoredContents)
    }

    @Test
    fun `GIVEN a sponsored content is persisted in the database WHEN the sponsored content is deleted THEN sponsored content is removed from the database`() = runTest {
        val sponsoredContent1 = PocketTestResources.sponsoredContentEntity
        val sponsoredContent2 = PocketTestResources.sponsoredContentEntity.copy(
            url = sponsoredContent1.url + "2",
        )

        dao.insertSponsoredContents(
            sponsoredContents = listOf(
                sponsoredContent1,
                sponsoredContent2,
            ),
        )
        dao.deleteSponsoredContents(sponsoredContents = listOf(sponsoredContent1))

        val sponsoredContents = dao.getSponsoredContents()

        assertEquals(listOf(sponsoredContent2), sponsoredContents)
    }

    @Test
    fun `WHEN all sponsored contents are deleted THEN all sponsored contents are removed from the database`() = runTest {
        val sponsoredContent1 = PocketTestResources.sponsoredContentEntity
        val sponsoredContent2 = PocketTestResources.sponsoredContentEntity.copy(
            url = sponsoredContent1.url + "2",
        )

        dao.insertSponsoredContents(
            sponsoredContents = listOf(
                sponsoredContent1,
                sponsoredContent2,
            ),
        )
        dao.deleteAllSponsoredContents()

        val sponsoredContents = dao.getSponsoredContents()

        assertTrue(sponsoredContents.isEmpty())
    }

    @Test
    fun `GIVEN sponsored contents are persisted in the database WHEN new sponsored contents are fetched and a clean and insert happens THEN new list of sponsored contents are persisted in the database`() = runTest {
        setupDatabaseForTransactions()
        val sponsoredContent1 = PocketTestResources.sponsoredContentEntity
        val sponsoredContent2 = PocketTestResources.sponsoredContentEntity.copy(
            url = sponsoredContent1.url + "2",
        )
        val sponsoredContent3 = PocketTestResources.sponsoredContentEntity.copy(
            url = sponsoredContent1.url + "3",
        )
        val sponsoredContent4 = PocketTestResources.sponsoredContentEntity.copy(
            url = sponsoredContent1.url + "4",
        )

        dao.insertSponsoredContents(
            sponsoredContents = listOf(
                sponsoredContent1,
                sponsoredContent2,
                sponsoredContent3,
            ),
        )

        dao.cleanOldAndInsertNewSponsoredContents(
            sponsoredContents = listOf(
                sponsoredContent2,
                sponsoredContent4,
            ),
        )

        val sponsoredContents = dao.getSponsoredContents()

        assertEquals(listOf(sponsoredContent2, sponsoredContent4), sponsoredContents)
    }

    @Test
    fun `GIVEN no sponsored contents are persisted in the database WHEN sponsored content impression is recorded THEN don't persist any data in the database`() = runTest {
        dao.recordImpression(targetUrl = PocketTestResources.sponsoredContentEntity.url)

        val impressions = dao.getSponsoredContentImpressions()

        assertTrue(impressions.isEmpty())
    }

    @Test
    fun `GIVEN sponsored contents are persisted in the database WHEN sponsored content impressions are recorded THEN persist impressions only for existing sponsored content`() = runTest {
        setupDatabaseForTransactions()
        val sponsoredContent1 = PocketTestResources.sponsoredContentEntity
        val sponsoredContent2 = PocketTestResources.sponsoredContentEntity.copy(
            url = sponsoredContent1.url + "2",
        )
        val sponsoredContent3 = PocketTestResources.sponsoredContentEntity.copy(
            url = sponsoredContent1.url + "3",
        )

        dao.insertSponsoredContents(
            sponsoredContents = listOf(
                sponsoredContent1,
                sponsoredContent3,
            ),
        )

        dao.recordImpressions(
            impressions = listOf(
                SponsoredContentImpressionEntity(sponsoredContent1.url),
                SponsoredContentImpressionEntity(sponsoredContent2.url),
                SponsoredContentImpressionEntity(sponsoredContent3.url),
            ),
        )

        val impressions = dao.getSponsoredContentImpressions()

        assertEquals(2, impressions.size)
        assertEquals(sponsoredContent1.url, impressions[0].url)
        assertEquals(sponsoredContent3.url, impressions[1].url)
    }

    /**
     * Sets an executor to be used for database transactions.
     * Needs to be used along with "runTest" to ensure waiting for transactions to finish but
     * not hang tests.
     */
    /**
     * Sets an executor to be used for database transactions.
     * Needs to be used along with "runTest" to ensure waiting for transactions to finish but
     * not hang tests.
     */
    private fun setupDatabaseForTransactions() {
        database = Room
            .inMemoryDatabaseBuilder(testContext, ContentRecommendationsDatabase::class.java)
            .setTransactionExecutor(executor)
            .allowMainThreadQueries()
            .build()
        dao = database.sponsoredContentsDao()
    }
}
