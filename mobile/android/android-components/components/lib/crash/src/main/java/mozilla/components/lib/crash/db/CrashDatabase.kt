/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import android.content.Context
import androidx.room.AutoMigration
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.TypeConverter
import androidx.room.TypeConverters
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json

/**
 * Internal database for storing collections and their tabs.
 */
@Database(
    entities = [CrashEntity::class, ReportEntity::class],
    version = 3,
    autoMigrations = [
        AutoMigration(from = 1, to = 2),
        AutoMigration(from = 2, to = 3),
    ],
)
@TypeConverters(Converter::class)
internal abstract class CrashDatabase : RoomDatabase() {
    abstract fun crashDao(): CrashDao

    companion object {
        @Volatile private var instance: CrashDatabase? = null

        @Synchronized
        fun get(context: Context): CrashDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context.applicationContext,
                CrashDatabase::class.java,
                "crashes",
            )
                // We are allowing main thread queries here since we need to write to disk blocking
                // in a crash event before the process gets shutdown. At this point the app already
                // crashed and temporarily blocking the UI thread is not that problematic anymore.
                .allowMainThreadQueries()
                .build()
                .also {
                    instance = it
                }
        }
    }
}

internal class Converter {
    private val mapSerializer = MapSerializer(String.serializer(), String.serializer())
    private val listSerializer = ListSerializer(String.serializer())

    @TypeConverter
    fun mapToString(map: Map<String, String>): String =
        Json.encodeToString(serializer = mapSerializer, value = map)

    @TypeConverter
    fun stringToMap(string: String): Map<String, String> =
        Json.decodeFromString(deserializer = mapSerializer, string = string)

    @TypeConverter
    fun listToString(strings: List<String>): String =
        Json.encodeToString(
            serializer = listSerializer,
            value = strings,
        )

    @TypeConverter
    fun stringToList(string: String): List<String> =
        Json.decodeFromString(deserializer = listSerializer, string = string)
}
