/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.GleanMetrics.SentFromFirefox
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Lets the user customize link sharing feature.
 */
class LinkSharingFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.link_sharing_preferences, rootKey)

        requirePreference<SwitchPreference>(R.string.pref_key_link_sharing).apply {
            isChecked = context.settings().whatsappLinkSharingEnabled
            onPreferenceChangeListener = SharedPreferenceUpdater()

            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    SentFromFirefox.settingsToggled.record(
                        SentFromFirefox.SettingsToggledExtra(
                            enabled = newValue as Boolean,
                        ),
                    )

                    return super.onPreferenceChange(preference, newValue)
                }
            }

            title = getString(
                R.string.link_sharing_toggle_body,
                getString(R.string.firefox),
            )
            summary = getString(
                R.string.link_sharing_toggle_title,
                getString(R.string.firefox),
            )
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_link_sharing))
    }
}
