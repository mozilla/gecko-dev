/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Intent
import android.os.Bundle
import android.provider.Settings
import android.view.WindowManager
import androidx.activity.result.ActivityResultLauncher
import androidx.biometric.BiometricManager
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.GleanMetrics.PrivateBrowsingLocked
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.ext.registerForActivityResult
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.settings.biometric.DefaultBiometricUtils
import org.mozilla.fenix.settings.biometric.ext.isAuthenticatorAvailable
import org.mozilla.fenix.settings.biometric.ext.isHardwareAvailable

/**
 * Lets the user customize Private browsing options.
 */
class PrivateBrowsingFragment : PreferenceFragmentCompat() {
    private lateinit var startForResult: ActivityResultLauncher<Intent>

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_private_browsing_options))

        // If user changes their device lock status (i.e. adds or removes device lock),
        // check the device pin status and determine if private browsing lock toggle
        // should be shown upon resuming.
        updatePreferences()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.private_browsing_preferences, rootKey)
        startForResult = registerForActivityResult(
            onFailure = { PrivateBrowsingLocked.authFailure.record() },
            onSuccess = { onSuccessfulAuthenticationUsingFallbackPrompt() },
        )
        updatePreferences()
    }

    private fun updatePreferences() {
        requirePreference<Preference>(R.string.pref_key_add_private_browsing_shortcut).apply {
            setOnPreferenceClickListener {
                PrivateShortcutCreateManager.createPrivateShortcut(requireContext())
                true
            }
        }

        requirePreference<SwitchPreference>(R.string.pref_key_open_links_in_a_private_tab).apply {
            onPreferenceChangeListener = SharedPreferenceUpdater()
            isChecked = context.settings().openLinksInAPrivateTab
        }

        requirePreference<SwitchPreference>(R.string.pref_key_allow_screenshots_in_private_mode).apply {
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    if ((activity as? HomeActivity)?.browsingModeManager?.mode?.isPrivate == true &&
                        newValue == false
                    ) {
                        activity?.window?.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
                    } else {
                        activity?.window?.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
                    }
                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        val biometricManager = BiometricManager.from(requireContext())
        val deviceCapable = biometricManager.isHardwareAvailable()
        val userHasEnabledCapability = biometricManager.isAuthenticatorAvailable()

        requirePreference<SwitchPreference>(R.string.pref_key_private_browsing_locked_enabled).apply {
            isChecked = context.settings().privateBrowsingLockedEnabled &&
                biometricManager.isAuthenticatorAvailable()
            isVisible = deviceCapable && FxNimbus.features.privateBrowsingLock.value().enabled
            isEnabled = userHasEnabledCapability

            setOnPreferenceChangeListener { preference, newValue ->
                val pbmLockEnabled = newValue as? Boolean
                    ?: return@setOnPreferenceChangeListener false

                val titleRes = if (pbmLockEnabled) {
                    R.string.pbm_authentication_enable_lock
                } else {
                    R.string.pbm_authentication_disable_lock
                }

                DefaultBiometricUtils.bindBiometricsCredentialsPromptOrShowWarning(
                    titleRes = titleRes,
                    view = requireView(),
                    onShowPinVerification = { intent -> startForResult.launch(intent) },
                    onAuthSuccess = {
                        onSuccessfulAuthenticationUsingPrimaryPrompt(
                            pbmLockEnabled = pbmLockEnabled,
                            preference = preference,
                        )
                    },
                    onAuthFailure = { PrivateBrowsingLocked.authFailure.record() },
                )

                PrivateBrowsingLocked.promptShown.record()

                // Cancel toggle change until biometric is successful
                false
            }
        }

        requirePreference<Preference>(R.string.pref_key_private_browsing_lock_device_feature_enabled).apply {
            isVisible =
                deviceCapable && !userHasEnabledCapability && FxNimbus.features.privateBrowsingLock.value().enabled

            setOnPreferenceClickListener {
                context.startActivity(Intent(Settings.ACTION_SECURITY_SETTINGS))
                true
            }
        }

        // Show bottom divider only if user does not have a device lock set
        requirePreference<PreferenceCategory>(R.string.pbm_lock_category_bottom_divider).apply {
            isVisible = !userHasEnabledCapability
        }
    }

    private fun onSuccessfulAuthenticationUsingFallbackPrompt() {
        PrivateBrowsingLocked.authSuccess.record()

        val newValue = !requireContext().settings().privateBrowsingLockedEnabled
        recordPbmLockFeatureEnabledStateTelemetry(newValue)
        requireContext().settings().privateBrowsingLockedEnabled = newValue
        // Update switch state manually
        requirePreference<SwitchPreference>(R.string.pref_key_private_browsing_locked_enabled).apply {
            isChecked = !isChecked
        }
    }

    private fun onSuccessfulAuthenticationUsingPrimaryPrompt(
        pbmLockEnabled: Boolean,
        preference: Preference,
    ) {
        PrivateBrowsingLocked.authSuccess.record()

        recordPbmLockFeatureEnabledStateTelemetry(pbmLockEnabled)
        requireContext().settings().privateBrowsingLockedEnabled = pbmLockEnabled
        // Update switch state manually
        (preference as? SwitchPreference)?.isChecked = pbmLockEnabled
    }

    private fun recordPbmLockFeatureEnabledStateTelemetry(pbmLockEnabled: Boolean) {
        if (pbmLockEnabled) {
            PrivateBrowsingLocked.featureEnabled.record()
        } else {
            PrivateBrowsingLocked.featureDisabled.record()
        }
    }
}
