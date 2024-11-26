/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import android.content.Context
import android.content.SharedPreferences
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.registerOnSharedPreferenceChangeListener

/**
 * Cache for accessing any settings related to privacy preferences.
 */
interface PrivacyPreferencesRepository {

    /**
     * Enum for all privacy preference keys.
     */
    enum class PrivacyPreference(@StringRes val preferenceKey: Int) {
        CrashReporting(preferenceKey = R.string.pref_key_crash_reporting_always_report),
        UsageData(preferenceKey = R.string.pref_key_telemetry),
    }

    /**
     * An update to a [PrivacyPreference].
     */
    data class PrivacyPreferenceUpdate(val preferenceType: PrivacyPreference, val value: Boolean)

    /**
     * A [Flow] of [PrivacyPreferenceUpdate]s.
     */
    val privacyPreferenceUpdates: Flow<PrivacyPreferenceUpdate>

    /**
     * Initializes the repository and starts the [SharedPreferences] listener.
     */
    fun init()

    /**
     * Update [PrivacyPreferenceUpdate.preferenceType] with [PrivacyPreferenceUpdate.value].
     */
    fun updatePrivacyPreference(preferenceUpdate: PrivacyPreferenceUpdate)
}

/**
 * Default implementation of [PrivacyPreferencesRepository].
 *
 * @param context the application context.
 * @param lifecycleOwner the lifecycle owner used for the SharedPreferences API.
 * @param coroutineScope the coroutine scope used for emitting flows.
 */
class DefaultPrivacyPreferencesRepository(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner,
    private val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : PrivacyPreferencesRepository {
    private val settings = context.settings()
    private val _privacyPreferenceUpdates =
        MutableSharedFlow<PrivacyPreferencesRepository.PrivacyPreferenceUpdate>()

    private fun emitPreferenceUpdate(
        privacyPreferenceUpdate: PrivacyPreferencesRepository.PrivacyPreferenceUpdate,
    ) = coroutineScope.launch { _privacyPreferenceUpdates.emit(privacyPreferenceUpdate) }

    override val privacyPreferenceUpdates: Flow<PrivacyPreferencesRepository.PrivacyPreferenceUpdate>
        get() = _privacyPreferenceUpdates.asSharedFlow()

    override fun init() {
        PrivacyPreferencesRepository.PrivacyPreference.entries.forEach { preference ->
            val initialPreferences = when (preference) {
                PrivacyPreferencesRepository.PrivacyPreference.CrashReporting ->
                    settings.crashReportAlwaysSend

                PrivacyPreferencesRepository.PrivacyPreference.UsageData ->
                    settings.isTelemetryEnabled
            }

            emitPreferenceUpdate(
                PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                    preferenceType = preference,
                    value = initialPreferences,
                ),
            )

            startListener()
        }
    }

    private fun startListener() {
        settings.preferences
            .registerOnSharedPreferenceChangeListener(owner = lifecycleOwner) { sharedPreferences, key ->
                onPreferenceChange(sharedPreferences = sharedPreferences, key = key)
            }
    }

    @VisibleForTesting
    internal fun onPreferenceChange(
        sharedPreferences: SharedPreferences,
        key: String?,
    ) {
        val preferenceType = PrivacyPreferencesRepository.PrivacyPreference.entries.find {
            context.getString(it.preferenceKey) == key
        } ?: return

        val privacyPreference = sharedPreferences.getBoolean(key, false)

        emitPreferenceUpdate(
            PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                preferenceType = preferenceType,
                value = privacyPreference,
            ),
        )
    }

    override fun updatePrivacyPreference(preferenceUpdate: PrivacyPreferencesRepository.PrivacyPreferenceUpdate) {
        when (preferenceUpdate.preferenceType) {
            PrivacyPreferencesRepository.PrivacyPreference.CrashReporting ->
                settings.crashReportAlwaysSend = preferenceUpdate.value

            PrivacyPreferencesRepository.PrivacyPreference.UsageData ->
                settings.isTelemetryEnabled = preferenceUpdate.value
        }
    }
}
