/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing content recommendations.
 */
@Database(
    entities = [
        ContentRecommendationEntity::class,
    ],
    version = 1,
)
internal abstract class ContentRecommendationsDatabase : RoomDatabase() {
    abstract fun contentRecommendationsDao(): ContentRecommendationsDao

    companion object {
        private const val DATABASE_NAME = "content_recommendations"
        internal const val CONTENT_RECOMMENDATIONS_TABLE = "content_recommendations"

        @Volatile
        private var instance: ContentRecommendationsDatabase? = null

        @Synchronized
        fun get(context: Context): ContentRecommendationsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                ContentRecommendationsDatabase::class.java,
                DATABASE_NAME,
            ).build().also {
                instance = it
            }
        }
    }
}
