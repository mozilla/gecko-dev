/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import org.mozilla.fenix.utils.Settings

/**
 * The repository for managing setup checklist preferences.
 */
interface SetupChecklistRepository {

    /**
     * Retrieves the state of a specific preference.
     *
     * @param type The type of preference to retrieve.
     * @return Returns `true` if the preference is enabled.
     */
    fun getPreference(type: PreferenceType): Boolean

    /**
     * Updates a specific preference.
     *
     * @param type The type of preference to modify.
     * @param enabled The new state of the preference.
     */
    fun setPreference(type: PreferenceType, enabled: Boolean)
}

/**
 * Enum representing the types of privacy preferences available.
 */
enum class PreferenceType {
    ToolbarComplete,
    ThemeComplete,
    ExtensionsComplete,
}

/**
 * The default implementation of [SetupChecklistRepository].
 *
 * @param settings The [Settings] instance for accessing and modifying setup checklist settings.
 */
class DefaultSetupChecklistRepository(
    private val settings: Settings,
) : SetupChecklistRepository {

    override fun getPreference(type: PreferenceType): Boolean {
        return when (type) {
            PreferenceType.ToolbarComplete -> settings.hasCompletedSetupStepToolbar
            PreferenceType.ThemeComplete -> settings.hasCompletedSetupStepTheme
            PreferenceType.ExtensionsComplete -> settings.hasCompletedSetupStepExtensions
        }
    }

    override fun setPreference(
        type: PreferenceType,
        enabled: Boolean,
    ) {
        when (type) {
            PreferenceType.ToolbarComplete -> settings.hasCompletedSetupStepToolbar = enabled
            PreferenceType.ThemeComplete -> settings.hasCompletedSetupStepTheme = enabled
            PreferenceType.ExtensionsComplete -> settings.hasCompletedSetupStepExtensions = enabled
        }
    }
}
