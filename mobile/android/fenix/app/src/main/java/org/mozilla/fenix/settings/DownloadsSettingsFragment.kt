/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Build
import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar

/**
 * A [PreferenceFragmentCompat] that displays settings related to downloads.
 */
class DownloadsSettingsFragment : PreferenceFragmentCompat() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.downloads_settings_preferences, rootKey)
        requirePreference<SwitchPreference>(R.string.pref_key_downloads_clean_up_files_automatically).apply {
            title = getString(
                R.string.preferences_downloads_settings_clean_up_files_title,
                Build.BRAND,
            )
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_downloads))
    }
}
