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
 * Cache for accessing any settings related to onboarding preferences.
 */
interface OnboardingPreferencesRepository {

    /**
     * Enum for the onboarding preference keys.
     */
    enum class OnboardingPreference(@StringRes val preferenceKey: Int) {
        DeviceTheme(preferenceKey = R.string.pref_key_follow_device_theme),
        LightTheme(preferenceKey = R.string.pref_key_light_theme),
        DarkTheme(preferenceKey = R.string.pref_key_dark_theme),
        TopToolbar(preferenceKey = R.string.pref_key_toolbar_top),
        BottomToolbar(preferenceKey = R.string.pref_key_toolbar_bottom),
    }

    /**
     * An update to a [OnboardingPreference].
     */
    data class OnboardingPreferenceUpdate(
        val preferenceType: OnboardingPreference,
        val value: Boolean = true,
    )

    /**
     * A [Flow] of [OnboardingPreferenceUpdate]s.
     */
    val onboardingPreferenceUpdates: Flow<OnboardingPreferenceUpdate>

    /**
     * Initializes the repository and starts the [SharedPreferences] listener.
     */
    fun init()

    /**
     * Update [OnboardingPreferenceUpdate.preferenceType] with [OnboardingPreferenceUpdate.value].
     */
    fun updateOnboardingPreference(preferenceUpdate: OnboardingPreferenceUpdate)
}

/**
 * Default implementation of [OnboardingPreferencesRepository].
 *
 * @param context the application context.
 * @param lifecycleOwner the lifecycle owner used for the SharedPreferences API.
 * @param coroutineScope the coroutine scope used for emitting flows.
 */
class DefaultOnboardingPreferencesRepository(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner,
    private val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : OnboardingPreferencesRepository {
    private val settings = context.settings()
    private val _onboardingPreferenceUpdates =
        MutableSharedFlow<OnboardingPreferencesRepository.OnboardingPreferenceUpdate>()

    private fun emitPreferenceUpdate(
        onboardingPreferenceUpdate: OnboardingPreferencesRepository.OnboardingPreferenceUpdate,
    ) = coroutineScope.launch { _onboardingPreferenceUpdates.emit(onboardingPreferenceUpdate) }

    override val onboardingPreferenceUpdates: Flow<OnboardingPreferencesRepository.OnboardingPreferenceUpdate>
        get() = _onboardingPreferenceUpdates.asSharedFlow()

    override fun init() {
        OnboardingPreferencesRepository.OnboardingPreference.entries.forEach { preference ->
            val initialPreferences = when (preference) {
                OnboardingPreferencesRepository.OnboardingPreference.DeviceTheme ->
                    settings.shouldFollowDeviceTheme

                OnboardingPreferencesRepository.OnboardingPreference.LightTheme ->
                    settings.shouldUseLightTheme

                OnboardingPreferencesRepository.OnboardingPreference.DarkTheme ->
                    settings.shouldUseDarkTheme

                OnboardingPreferencesRepository.OnboardingPreference.TopToolbar ->
                    !settings.shouldUseBottomToolbar

                OnboardingPreferencesRepository.OnboardingPreference.BottomToolbar ->
                    settings.shouldUseBottomToolbar
            }

            emitPreferenceUpdate(
                OnboardingPreferencesRepository.OnboardingPreferenceUpdate(
                    preferenceType = preference,
                    value = initialPreferences,
                ),
            )
        }

        coroutineScope.launch {
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
        val preferenceType = OnboardingPreferencesRepository.OnboardingPreference.entries.find {
            context.getString(it.preferenceKey) == key
        } ?: return

        val onboardingPreference = sharedPreferences.getBoolean(key, false)

        emitPreferenceUpdate(
            OnboardingPreferencesRepository.OnboardingPreferenceUpdate(
                preferenceType = preferenceType,
                value = onboardingPreference,
            ),
        )
    }

    override fun updateOnboardingPreference(
        preferenceUpdate: OnboardingPreferencesRepository.OnboardingPreferenceUpdate,
    ) = with(preferenceUpdate) {
        when (preferenceType) {
            OnboardingPreferencesRepository.OnboardingPreference.DeviceTheme ->
                updateSettingsToFollowSystemTheme()

            OnboardingPreferencesRepository.OnboardingPreference.LightTheme ->
                updateSettingsToLightTheme()

            OnboardingPreferencesRepository.OnboardingPreference.DarkTheme ->
                updateSettingsToDarkTheme()

            OnboardingPreferencesRepository.OnboardingPreference.TopToolbar ->
                settings.shouldUseBottomToolbar = false

            OnboardingPreferencesRepository.OnboardingPreference.BottomToolbar ->
                settings.shouldUseBottomToolbar = true
        }
    }

    @VisibleForTesting
    internal fun updateSettingsToFollowSystemTheme() =
        updateAllThemeOptions(followSystemTheme = true)

    @VisibleForTesting
    internal fun updateSettingsToLightTheme() = updateAllThemeOptions(useLightTheme = true)

    @VisibleForTesting
    internal fun updateSettingsToDarkTheme() = updateAllThemeOptions(useDarkTheme = true)

    private fun updateAllThemeOptions(
        followSystemTheme: Boolean = false,
        useLightTheme: Boolean = false,
        useDarkTheme: Boolean = false,
    ) {
        settings.shouldFollowDeviceTheme = followSystemTheme
        settings.shouldUseLightTheme = useLightTheme
        settings.shouldUseDarkTheme = useDarkTheme
    }
}
