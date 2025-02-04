/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.DialogFragment
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.ManagePrivacyPreferencesDialog
import org.mozilla.fenix.onboarding.store.DefaultPrivacyPreferencesRepository
import org.mozilla.fenix.onboarding.store.PreferenceType
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesAction
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesMiddleware
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesState
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesStore
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesTelemetryMiddleware
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.launchSandboxCustomTab
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Dialog fragment for managing privacy preferences.
 */
class ManagePrivacyPreferencesDialogFragment : DialogFragment() {

    private val store by lazyStore {
        val repository = DefaultPrivacyPreferencesRepository(
            settings = requireContext().settings(),
        )
        PrivacyPreferencesStore(
            initialState = PrivacyPreferencesState(
                crashReportingEnabled = repository.getPreference(PreferenceType.CrashReporting),
                usageDataEnabled = repository.getPreference(PreferenceType.UsageData),
            ),
            middlewares = listOf(
                PrivacyPreferencesMiddleware(repository),
                PrivacyPreferencesTelemetryMiddleware(),
            ),
        )
    }

    private val crashReportingUrl by lazy { sumoUrlFor(SupportUtils.SumoTopic.CRASH_REPORTS) }
    private val usageDataUrl by lazy { sumoUrlFor(SupportUtils.SumoTopic.TECHNICAL_AND_INTERACTION_DATA) }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            FirefoxTheme {
                ManagePrivacyPreferencesDialog(
                    store = store,
                    onDismissRequest = { dismiss() },
                    onCrashReportingLinkClick = {
                        store.dispatch(PrivacyPreferencesAction.CrashReportingLearnMore)
                        launchSandboxCustomTab(requireContext(), crashReportingUrl)
                    },
                    onUsageDataLinkClick = {
                        store.dispatch(PrivacyPreferencesAction.UsageDataUserLearnMore)
                        launchSandboxCustomTab(requireContext(), usageDataUrl)
                    },
                )
            }
        }
    }

    private fun sumoUrlFor(topic: SupportUtils.SumoTopic) =
        SupportUtils.getSumoURLForTopic(requireContext(), topic)

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
