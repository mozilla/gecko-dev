/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import org.mozilla.fenix.utils.Settings

/**
 * The repository for managing user privacy preferences set during onboarding.
 */
interface SimplePrivacyPreferencesRepository {

    /**
     * Retrieves the state of a specific preference.
     *
     * @param type The type of preference to retrieve.
     * @return Returns `true` if the preference is enabled.
     */
    fun getPrivacyPreference(type: PreferenceType): Boolean

    /**
     * Updates a specific preference.
     *
     * @param type The type of preference to modify.
     * @param enabled The new state of the preference.
     */
    fun setPrivacyPreference(type: PreferenceType, enabled: Boolean)
}

/**
 * Enum representing the types of privacy preferences available.
 */
enum class PreferenceType {
    CrashReporting, UsageData,
}

/**
 * The default implementation of [SimplePrivacyPreferencesRepository].
 *
 * @param settings The [Settings] instance for accessing and modifying privacy-related settings.
 */
class DefaultSimplePrivacyPreferencesRepository(
    private val settings: Settings,
) : SimplePrivacyPreferencesRepository {

    override fun getPrivacyPreference(type: PreferenceType): Boolean {
        return when (type) {
            PreferenceType.CrashReporting -> settings.crashReportAlwaysSend
            PreferenceType.UsageData -> settings.isTelemetryEnabled
        }
    }

    override fun setPrivacyPreference(
        type: PreferenceType,
        enabled: Boolean,
    ) {
        when (type) {
            PreferenceType.CrashReporting -> settings.crashReportAlwaysSend = enabled
            PreferenceType.UsageData -> settings.isTelemetryEnabled = enabled
        }
    }
}
