/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.addresses

import android.content.Context
import android.content.Context.MODE_PRIVATE
import android.content.SharedPreferences

private const val SHARED_PREFS_FILENAME = "ADDRESSES_DEBUG_LOCALES"

/**
 * List of locales that can be enabled for debuging purposes only.
 */
// Lang tags found at https://gist.github.com/typpo/b2b828a35e683b9bf8db91b5404f1bd1
enum class DebugLocale(val langTag: String) {
    DE("de-DE"),
    FR("fr-FR"),
}

/**
 * Type declaring methods for interacting with a storage layer relating to debug locales.
 */
interface AddressesDebugLocalesRepository {
    /**
     * Get all the enabled debug locale.
     */
    fun getAllEnabledLocales(): List<DebugLocale>

    /**
     * Check whether a debug locale is enabled.
     */
    fun isLocaleEnabled(locale: DebugLocale): Boolean

    /**
     * Set whether a debug locale is enabled.
     */
    fun setLocaleEnabled(locale: DebugLocale, enabled: Boolean)
}

/**
 * A [AddressesDebugLocalesRepository] that uses shared prefs as its storage mechanism. This was chosen
 * for easy interop with utils/Settings.kt but could be updated to DataStore down the road.
 */
class SharedPrefsAddressesDebugLocalesRepository(
    context: Context,
) : AddressesDebugLocalesRepository {

    private val prefs: SharedPreferences = context.getSharedPreferences(SHARED_PREFS_FILENAME, MODE_PRIVATE)

    override fun getAllEnabledLocales(): List<DebugLocale> =
        DebugLocale.entries.filter { locale ->
            prefs.getBoolean(locale.name, false)
        }

    override fun isLocaleEnabled(locale: DebugLocale): Boolean =
        prefs.getBoolean(locale.name, false)

    override fun setLocaleEnabled(locale: DebugLocale, enabled: Boolean) =
        prefs.edit().putBoolean(locale.name, enabled).apply()
}

/**
 * A fake [AddressesDebugLocalesRepository].
 */
class FakeAddressesDebugLocalesRepository : AddressesDebugLocalesRepository {
    private val debugLocales = DebugLocale.entries.associateWith {
        false
    }.toMutableMap()

    override fun getAllEnabledLocales(): List<DebugLocale> {
        return debugLocales.filter { it.value }.keys.toList()
    }

    override fun isLocaleEnabled(locale: DebugLocale): Boolean =
        debugLocales[locale] ?: false

    override fun setLocaleEnabled(locale: DebugLocale, enabled: Boolean) {
        debugLocales[locale] = enabled
    }
}
