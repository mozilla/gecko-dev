/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ids

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit

private const val KEY_NEXT_ID = "nextId"
private const val KEY_LAST_USED_PREFIX = "lastUsed."
private const val KEY_ID_PREFIX = "id."

/**
 * Internal helper to create unique and stable [Int] IDs based on [String] tags.
 *
 * @param fileName The shared preference file that should be used to save ID assignments.
 * @param idLifeTime The maximum time an ID can be unused until it is cleared.
 * @param offset The [Int] offset from which this instance should start providing IDs.
 */
internal class SharedIds(
    private val fileName: String,
    private val idLifeTime: Long,
    private val offset: Int = 0,
) {
    /**
     * Get a unique ID for the provided unique tag.
     */
    @Synchronized
    fun getIdForTag(context: Context, tag: String): Int {
        val preferences = preferences(context)
        val key = tagToKey(tag)
        val lastUsedKey = tagToLastUsedKey(tag)

        removeExpiredIds(preferences)

        // First, check if we already have an ID for this tag
        val existingId = preferences.getInt(key, -1)
        if (existingId != -1) {
            // Update the last used timestamp
            preferences.edit { putLong(lastUsedKey, now()) }
            return existingId
        }

        // If no ID exists, assign the next available one
        val nextId = preferences.getInt(KEY_NEXT_ID, offset)
        preferences.edit {
            putInt(KEY_NEXT_ID, nextId + 1) // Increment the next ID, ignoring overflow for now
            putInt(key, nextId) // Assign the current next ID to the tag
            putLong(lastUsedKey, now()) // Update the last used timestamp
        }

        return nextId
    }

    /**
     * Get the next available unique ID for the provided unique tag.
     */
    @Synchronized
    fun getNextIdForTag(context: Context, tag: String): Int {
        val preferences = preferences(context)
        val key = tagToKey(tag)
        val lastUsedKey = tagToLastUsedKey(tag)

        removeExpiredIds(preferences)

        // Always use the next available one and save that
        val nextId = preferences.getInt(KEY_NEXT_ID, offset)
        preferences.edit {
            putInt(KEY_NEXT_ID, nextId + 1) // Increment the next ID, ignoring overflow for now
            putInt(key, nextId) // Assign the current next ID to the tag
            putLong(lastUsedKey, now()) // Update the last used timestamp
        }

        return nextId
    }

    private fun tagToKey(tag: String): String {
        return "$KEY_ID_PREFIX.$tag"
    }

    private fun tagToLastUsedKey(tag: String): String {
        return "$KEY_LAST_USED_PREFIX$tag"
    }

    private fun preferences(context: Context): SharedPreferences {
        return context.getSharedPreferences(fileName, Context.MODE_PRIVATE)
    }

    /**
     * Remove all expired notification IDs.
     *
     * @param preferences The [SharedPreferences] instance.
     */
    private fun removeExpiredIds(preferences: SharedPreferences) {
        val expiredEntries = preferences.all.entries
            .filter { it.key.startsWith(KEY_LAST_USED_PREFIX) }
            .filter {
                val lastUsed = it.value as? Long
                lastUsed != null && lastUsed < (now() - idLifeTime)
            }

        if (expiredEntries.isNotEmpty()) {
            preferences.edit {
                expiredEntries.forEach { entry ->
                    val lastUsedKey = entry.key
                    val tag = lastUsedKey.removePrefix(KEY_LAST_USED_PREFIX)
                    remove(tagToKey(tag))
                    remove(lastUsedKey)
                }
            }
        }
    }

    fun clear(context: Context) { preferences(context).edit { clear() } }

    internal var now: () -> Long = { System.currentTimeMillis() }
}
