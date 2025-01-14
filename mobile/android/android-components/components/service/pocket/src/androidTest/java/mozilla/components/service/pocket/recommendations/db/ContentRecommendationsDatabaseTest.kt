/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.db

import android.content.Context
import androidx.room.Room
import androidx.room.testing.MigrationTestHelper
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test

private const val MIGRATION_TEST_DB = "migration-test"

class ContentRecommendationsDatabaseTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: ContentRecommendationsDatabase

    @get:Rule
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        ContentRecommendationsDatabase::class.java,
    )

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, ContentRecommendationsDatabase::class.java)
            .build()
    }

    @After
    fun tearDown() {
        database.close()
    }

    @Test
    fun migrate1to2() {
        helper.createDatabase(MIGRATION_TEST_DB, 1).apply {
            execSQL(
                """
                    INSERT INTO ${ContentRecommendationsDatabase.CONTENT_RECOMMENDATIONS_TABLE}
                    (scheduledCorpusItemId, url, title, excerpt, topic, publisher, isTimeSensitive, imageUrl, tileId, receivedRank, impressions)
                    VALUES (
                    "${contentRecommendationEntity.scheduledCorpusItemId}",
                    "${contentRecommendationEntity.url}",
                    "${contentRecommendationEntity.title}",
                    "${contentRecommendationEntity.excerpt}",
                    "${contentRecommendationEntity.topic}",
                    "${contentRecommendationEntity.publisher}",
                    "${contentRecommendationEntity.isTimeSensitive}",
                    "${contentRecommendationEntity.imageUrl}",
                    "${contentRecommendationEntity.tileId}",
                    "${contentRecommendationEntity.receivedRank}",
                    "${contentRecommendationEntity.impressions}"
                    )
                """,
            )

            query("SELECT * FROM ${ContentRecommendationsDatabase.CONTENT_RECOMMENDATIONS_TABLE}")
                .use { cursor ->
                    assertEquals(11, cursor.columnCount)
                    assertEquals(1, cursor.count)

                    cursor.moveToFirst()

                    assertEquals(
                        contentRecommendationEntity.scheduledCorpusItemId,
                        cursor.getString(cursor.getColumnIndexOrThrow("scheduledCorpusItemId")),
                    )
                }
        }

        helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB,
            2,
            true,
            Migrations.migration_1_2,
        ).apply {
            query("SELECT * FROM ${ContentRecommendationsDatabase.CONTENT_RECOMMENDATIONS_TABLE}")
                .use { cursor ->
                    assertEquals(13, cursor.columnCount)
                    assertEquals(0, cursor.count)
                }

            execSQL(
                """
                    INSERT INTO ${ContentRecommendationsDatabase.CONTENT_RECOMMENDATIONS_TABLE}
                    (corpusItemId, scheduledCorpusItemId, url, title, excerpt, topic, publisher, isTimeSensitive, imageUrl, tileId, receivedRank, recommendedAt, impressions)
                    VALUES (
                    "${contentRecommendationEntity.corpusItemId}",
                    "${contentRecommendationEntity.scheduledCorpusItemId}",
                    "${contentRecommendationEntity.url}",
                    "${contentRecommendationEntity.title}",
                    "${contentRecommendationEntity.excerpt}",
                    "${contentRecommendationEntity.topic}",
                    "${contentRecommendationEntity.publisher}",
                    "${contentRecommendationEntity.isTimeSensitive}",
                    "${contentRecommendationEntity.imageUrl}",
                    "${contentRecommendationEntity.tileId}",
                    "${contentRecommendationEntity.receivedRank}",
                    "${contentRecommendationEntity.recommendedAt}",
                    "${contentRecommendationEntity.impressions}"
                    )
                """,
            )

            query("SELECT * FROM ${ContentRecommendationsDatabase.CONTENT_RECOMMENDATIONS_TABLE}")
                .use { cursor ->
                    assertEquals(1, cursor.count)

                    cursor.moveToFirst()

                    assertEquals(
                        contentRecommendationEntity.corpusItemId,
                        cursor.getString(cursor.getColumnIndexOrThrow("corpusItemId")),
                    )
                    assertEquals(
                        contentRecommendationEntity.recommendedAt,
                        cursor.getLong(cursor.getColumnIndexOrThrow("recommendedAt")),
                    )
                }
        }
    }
}

private val contentRecommendationEntity = ContentRecommendationEntity(
    corpusItemId = "1111",
    scheduledCorpusItemId = "2222",
    url = "https://getpocket.com/",
    title = "Pocket",
    excerpt = "Pocket",
    topic = "food",
    publisher = "Pocket",
    isTimeSensitive = false,
    imageUrl = "https://img-getpocket.cdn.mozilla.net/",
    tileId = 1,
    receivedRank = 2,
    recommendedAt = 1L,
    impressions = 1,
)
