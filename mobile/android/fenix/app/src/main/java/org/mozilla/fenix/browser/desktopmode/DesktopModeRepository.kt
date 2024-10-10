/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.desktopmode

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import org.mozilla.fenix.datastore.editOrCatch
import org.mozilla.fenix.datastore.preferencesDataStore
import org.mozilla.fenix.utils.isLargeScreenSize

private const val DESKTOP_BROWSING_KEY = "desktop_browsing_key"

private val desktopBrowsingEnabledKey = booleanPreferencesKey(DESKTOP_BROWSING_KEY)

/**
 * Cache for accessing any settings related to the desktop mode feature.
 */
interface DesktopModeRepository {

    /**
     * Whether browsing is in desktop mode by default for any newly opened tabs.
     */
    suspend fun getDesktopBrowsingEnabled(): Boolean

    /**
     * Updates whether desktop browsing by default is enabled.
     *
     * @param enabled Whether the desktop browsing is to be enabled.
     */
    suspend fun setDesktopBrowsingEnabled(enabled: Boolean): Boolean
}

/**
 * The default implementation of [DesktopModeRepository].
 *
 * @param context Android context used to obtain the underlying [DataStore].
 * @param dataStore [DataStore] for accessing user preferences.
 */
class DefaultDesktopModeRepository(
    context: Context,
    private val dataStore: DataStore<Preferences> = context.preferencesDataStore,
) : DesktopModeRepository {

    private val defaultDesktopMode by lazy {
        context.isLargeScreenSize()
    }

    override suspend fun getDesktopBrowsingEnabled(): Boolean =
        dataStore.data.map { preferences ->
            preferences[desktopBrowsingEnabledKey] ?: defaultDesktopMode
        }.first()

    override suspend fun setDesktopBrowsingEnabled(enabled: Boolean): Boolean {
        var preferenceWriteSucceeded = true
        dataStore.editOrCatch(
            onError = {
                preferenceWriteSucceeded = false
            },
        ) { preferences ->
            preferences[desktopBrowsingEnabledKey] = enabled
        }

        return preferenceWriteSucceeded
    }
}
