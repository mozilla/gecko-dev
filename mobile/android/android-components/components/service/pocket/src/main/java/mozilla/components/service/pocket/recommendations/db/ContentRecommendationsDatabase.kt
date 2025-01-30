/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import mozilla.components.service.pocket.mars.db.SponsoredContentEntity
import mozilla.components.service.pocket.mars.db.SponsoredContentImpressionEntity
import mozilla.components.service.pocket.mars.db.SponsoredContentsDao
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase.Companion.CONTENT_RECOMMENDATIONS_TABLE
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase.Companion.SPONSORED_CONTENT_IMPRESSION_TABLE
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDatabase.Companion.SPONSORED_CONTENT_TABLE

/**
 * Internal database for storing content recommendations.
 */
@Database(
    entities = [
        ContentRecommendationEntity::class,
        SponsoredContentEntity::class,
        SponsoredContentImpressionEntity::class,
    ],
    version = 3,
)
internal abstract class ContentRecommendationsDatabase : RoomDatabase() {
    abstract fun contentRecommendationsDao(): ContentRecommendationsDao
    abstract fun sponsoredContentsDao(): SponsoredContentsDao

    companion object {
        private const val DATABASE_NAME = "content_recommendations"
        internal const val CONTENT_RECOMMENDATIONS_TABLE = "content_recommendations"
        internal const val SPONSORED_CONTENT_TABLE = "sponsored_content"
        internal const val SPONSORED_CONTENT_IMPRESSION_TABLE = "sponsored_content_impressions"

        @Volatile
        private var instance: ContentRecommendationsDatabase? = null

        @Synchronized
        fun get(context: Context): ContentRecommendationsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                ContentRecommendationsDatabase::class.java,
                DATABASE_NAME,
            ).addMigrations(
                Migrations.migration_1_2,
                Migrations.migration_2_3,
            ).build().also {
                instance = it
            }
        }
    }
}

internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(db: SupportSQLiteDatabase) {
            // Drop the old table.
            db.execSQL("DROP TABLE $CONTENT_RECOMMENDATIONS_TABLE")

            // Create the version 2 table.
            db.execSQL(
                """
                    CREATE TABLE IF NOT EXISTS `$CONTENT_RECOMMENDATIONS_TABLE` (
                    `corpusItemId` TEXT NOT NULL,
                    `scheduledCorpusItemId` TEXT NOT NULL,
                    `url` TEXT NOT NULL,
                    `title` TEXT NOT NULL,
                    `excerpt` TEXT NOT NULL,
                    `topic` TEXT,
                    `publisher` TEXT NOT NULL,
                    `isTimeSensitive` INTEGER NOT NULL,
                    `imageUrl` TEXT NOT NULL,
                    `tileId` INTEGER NOT NULL,
                    `receivedRank` INTEGER NOT NULL,
                    `recommendedAt` INTEGER NOT NULL,
                    `impressions` INTEGER NOT NULL,
                    PRIMARY KEY(`corpusItemId`))
                """,
            )
        }
    }

    val migration_2_3 = object : Migration(2, 3) {
        override fun migrate(db: SupportSQLiteDatabase) {
            db.execSQL(
                """
                    CREATE TABLE IF NOT EXISTS `$SPONSORED_CONTENT_TABLE` (
                    `url` TEXT NOT NULL,
                    `title` TEXT NOT NULL,
                    `clickUrl` TEXT NOT NULL,
                    `impressionUrl` TEXT NOT NULL,
                    `imageUrl` TEXT NOT NULL,
                    `domain` TEXT NOT NULL,
                    `excerpt` TEXT NOT NULL,
                    `sponsor` TEXT NOT NULL,
                    `blockKey` TEXT NOT NULL,
                    `flightCapCount` INTEGER NOT NULL,
                    `flightCapPeriod` INTEGER NOT NULL,
                    `priority` INTEGER NOT NULL,
                    PRIMARY KEY(`url`))
                """,
            )

            db.execSQL(
                """
                    CREATE TABLE IF NOT EXISTS `$SPONSORED_CONTENT_IMPRESSION_TABLE` (
                    `url` TEXT NOT NULL,
                    `impressionId` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
                    `impressionDateInSeconds` INTEGER NOT NULL,
                    FOREIGN KEY(`url`) REFERENCES `$SPONSORED_CONTENT_TABLE`(`url`) ON UPDATE NO ACTION ON DELETE CASCADE )
                """,
            )

            db.execSQL(
                """
                    CREATE INDEX IF NOT EXISTS `index_sponsored_content_impressions_url`
                    ON `$SPONSORED_CONTENT_IMPRESSION_TABLE` (`url`)
                """,
            )
        }
    }
}
