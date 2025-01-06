/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.cfrs

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
 * Cache for accessing any settings related to CFR visibility.
 */
interface CfrPreferencesRepository {

    /**
     * An enum for all CFR pref keys
     */
    enum class CfrPreference(
        @StringRes val preferenceKey: Int,
    ) {
        HomepageSync(preferenceKey = R.string.pref_key_should_show_sync_cfr),
        HomepageNavToolbar(preferenceKey = R.string.pref_key_should_navbar_cfr),
        HomepageSearchBar(preferenceKey = R.string.pref_key_should_searchbar_cfr),
        NavButtons(preferenceKey = R.string.pref_key_toolbar_navigation_cfr),
        AddPrivateTabToHome(preferenceKey = R.string.pref_key_showed_private_mode_cfr),
        TabAutoCloseBanner(preferenceKey = R.string.pref_key_should_show_auto_close_tabs_banner),
        InactiveTabs(preferenceKey = R.string.pref_key_should_show_inactive_tabs_popup),
        OpenInApp(preferenceKey = R.string.pref_key_should_show_open_in_app_banner),
    }

    /**
     * An update to a [CfrPreference].
     */
    data class CfrPreferenceUpdate(
        val preferenceType: CfrPreference,
        val value: Boolean,
    )

    /**
     * A [Flow] of [CfrPreferenceUpdate]s.
     */
    val cfrPreferenceUpdates: Flow<CfrPreferenceUpdate>

    /**
     * Initializes the repository and starts the [SharedPreferences] listener.
     */
    fun init()

    /**
     * Update [CfrPreferenceUpdate.preferenceType] with [CfrPreferenceUpdate.value].
     */
    fun updateCfrPreference(preferenceUpdate: CfrPreferenceUpdate)

    /**
     * Reset lastCfrShownTimeInMillis to 0.
     */
    fun resetLastCfrTimestamp()
}

/**
 * The default implementation of [DefaultCfrPreferencesRepository].
 *
 * @param context the Android context.
 * @param lifecycleOwner the lifecycle owner used for the SharedPreferences API.
 * @param coroutineScope the coroutine scope used for emitting flows.
 */
class DefaultCfrPreferencesRepository(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner,
    private val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : CfrPreferencesRepository {
    private val settings = context.settings()
    private val _cfrPreferenceUpdates = MutableSharedFlow<CfrPreferencesRepository.CfrPreferenceUpdate>()

    @VisibleForTesting
    internal fun submitPreferenceUpdate(
        cfrPreferenceUpdate: CfrPreferencesRepository.CfrPreferenceUpdate,
    ) = coroutineScope.launch {
        _cfrPreferenceUpdates.emit(cfrPreferenceUpdate)
    }

    override val cfrPreferenceUpdates: Flow<CfrPreferencesRepository.CfrPreferenceUpdate>
        get() = _cfrPreferenceUpdates.asSharedFlow()

    override fun init() {
        CfrPreferencesRepository.CfrPreference.entries.forEach { preference ->
            val initialPreferenceValue = when (preference) {
                CfrPreferencesRepository.CfrPreference.HomepageSync ->
                    settings.showSyncCFR
                CfrPreferencesRepository.CfrPreference.HomepageNavToolbar ->
                    settings.shouldShowNavigationBarCFR
                CfrPreferencesRepository.CfrPreference.HomepageSearchBar ->
                    settings.shouldShowSearchBarCFR
                CfrPreferencesRepository.CfrPreference.NavButtons ->
                    settings.shouldShowNavigationButtonsCFR
                CfrPreferencesRepository.CfrPreference.AddPrivateTabToHome ->
                    settings.showedPrivateModeContextualFeatureRecommender
                CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner ->
                    settings.shouldShowAutoCloseTabsBanner
                CfrPreferencesRepository.CfrPreference.InactiveTabs ->
                    settings.shouldShowInactiveTabsOnboardingPopup
                CfrPreferencesRepository.CfrPreference.OpenInApp -> {
                    settings.shouldShowOpenInAppBanner
                }
            }

            submitPreferenceUpdate(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = preference,
                    value = initialPreferenceValue,
                ),
            )

            startListener()
        }
    }

    private fun startListener() {
        settings.preferences.registerOnSharedPreferenceChangeListener(
            owner = lifecycleOwner,
        ) { sharedPreferences, key ->
            onPreferenceChange(sharedPreferences = sharedPreferences, key = key)
        }
    }

    @VisibleForTesting
    internal fun onPreferenceChange(
        sharedPreferences: SharedPreferences,
        key: String?,
    ) {
        val preferenceType = CfrPreferencesRepository.CfrPreference.entries.find {
            context.getString(it.preferenceKey) == key
        } ?: return

        val cfrPreference = sharedPreferences.getBoolean(key, false)

        submitPreferenceUpdate(
            CfrPreferencesRepository.CfrPreferenceUpdate(
                preferenceType = preferenceType,
                value = cfrPreference,
            ),
        )
    }

    override fun updateCfrPreference(preferenceUpdate: CfrPreferencesRepository.CfrPreferenceUpdate) {
        // Note that after CFR pref values gets renamed, this code block will be condensed into 3 lines.
        // The implementation has been done like this in favour of the deferred CFR toggles, that
        // will require toggling more than 1 pref value or has inverted logic.
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1916992 for more details.
        when (preferenceUpdate.preferenceType) {
            CfrPreferencesRepository.CfrPreference.HomepageSync ->
                settings.showSyncCFR = !preferenceUpdate.value
            CfrPreferencesRepository.CfrPreference.HomepageNavToolbar ->
                settings.shouldShowNavigationBarCFR = !preferenceUpdate.value
            CfrPreferencesRepository.CfrPreference.HomepageSearchBar ->
                settings.shouldShowSearchBarCFR = !preferenceUpdate.value
            CfrPreferencesRepository.CfrPreference.NavButtons ->
                settings.shouldShowNavigationButtonsCFR = !preferenceUpdate.value
            CfrPreferencesRepository.CfrPreference.AddPrivateTabToHome -> {
                // This will be implemented at a later date due to its complex nature.
                // See https://bugzilla.mozilla.org/show_bug.cgi?id=1916830 for more details.
            }
            CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner ->
                settings.shouldShowAutoCloseTabsBanner = !preferenceUpdate.value
            CfrPreferencesRepository.CfrPreference.InactiveTabs ->
                settings.shouldShowInactiveTabsOnboardingPopup = !preferenceUpdate.value
            CfrPreferencesRepository.CfrPreference.OpenInApp -> {
                settings.shouldShowOpenInAppBanner = !preferenceUpdate.value
            }
        }
    }

    override fun resetLastCfrTimestamp() {
        settings.lastCfrShownTimeInMillis = 0
    }
}
