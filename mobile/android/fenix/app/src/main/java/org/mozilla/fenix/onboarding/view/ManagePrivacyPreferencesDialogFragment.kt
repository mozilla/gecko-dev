/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import android.app.Dialog
import android.os.Bundle
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.DialogFragment
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.onboarding.ManagePrivacyPreferencesDialog
import org.mozilla.fenix.onboarding.store.DefaultPrivacyPreferencesRepository
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesMiddleware
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesStore
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesTelemetryMiddleware
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Dialog fragment for managing privacy preferences.
 */
class ManagePrivacyPreferencesDialogFragment(
    private val onCrashReportingLinkClick: () -> Unit,
    private val onUsageDataLinkClick: () -> Unit,
) : DialogFragment() {

    private val store by lazyStore {
        PrivacyPreferencesStore(
            middlewares = listOf(
                PrivacyPreferencesMiddleware(
                    privacyPreferencesRepository = DefaultPrivacyPreferencesRepository(
                        context = requireContext(),
                        lifecycleOwner = viewLifecycleOwner,
                    ),
                ),
                PrivacyPreferencesTelemetryMiddleware(),
            ),
        )
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return Dialog(requireContext()).apply {
            setContentView(
                ComposeView(requireContext()).apply {
                    setContent {
                        FirefoxTheme {
                            ManagePrivacyPreferencesDialog(
                                store = store,
                                onDismissRequest = { dismiss() },
                                onCrashReportingLinkClick = onCrashReportingLinkClick,
                                onUsageDataLinkClick = onUsageDataLinkClick,
                            )
                        }
                    }
                },
            )
        }
    }

    /**
     * Companion object for [ManagePrivacyPreferencesDialogFragment].
     */
    companion object {
        /**
         * Tag for the [ManagePrivacyPreferencesDialogFragment].
         */
        const val TAG = "Privacy preferences dialog"
    }
}
