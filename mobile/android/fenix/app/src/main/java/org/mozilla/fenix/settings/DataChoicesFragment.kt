/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.Fragment
import androidx.navigation.findNavController
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.store.CrashReportOption
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricServiceType
import org.mozilla.fenix.crashes.SettingsCrashReportCache
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Lets the user toggle telemetry on/off.
 */
class DataChoicesFragment : Fragment() {

    private val crashReportCache by lazy {
        SettingsCrashReportCache(requireContext().settings())
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        return ComposeView(requireContext()).apply {
            setContent {
                FirefoxTheme {
                    var telemetryEnabled by remember { mutableStateOf(context.settings().isTelemetryEnabled) }
                    var usagePingEnabled by remember { mutableStateOf(context.settings().isDailyUsagePingEnabled) }
                    var measurementDataEnabled by remember {
                        mutableStateOf(context.settings().isMarketingTelemetryEnabled)
                    }
                    var selectedCrashOption by remember { mutableStateOf(CrashReportOption.Ask) }
                    val scope = rememberCoroutineScope()
                    LaunchedEffect(Unit) {
                        selectedCrashOption = crashReportCache.getReportOption()
                    }

                    DataChoicesScreen(
                        params = DataChoicesParams(
                            telemetryEnabled = telemetryEnabled,
                            usagePingEnabled = usagePingEnabled,
                            studiesEnabled = context.settings().isExperimentationEnabled,
                            measurementDataEnabled = measurementDataEnabled,
                            selectedCrashOption = selectedCrashOption,
                        ),
                        onTelemetryToggle = { newValue ->
                            scope.launch {
                                updateTelemetryChoice(newValue, context)
                            }
                            telemetryEnabled = newValue
                        },
                        onUsagePingToggle = { newValue ->
                            scope.launch {
                                updateUsageChoice(newValue, context)
                            }
                            usagePingEnabled = newValue
                        },
                        onMarketingDataToggled = { newValue ->
                            scope.launch {
                                updateMarketingDataChoice(newValue, context)
                            }
                            measurementDataEnabled = newValue
                        },
                        onCrashOptionSelected = { newValue ->
                            scope.launch {
                                updateCrashChoice(newValue, context)
                            }
                            selectedCrashOption = newValue
                        },
                        onStudiesClick = {
                            val action = DataChoicesFragmentDirections.actionDataChoicesFragmentToStudiesFragment()
                            view?.findNavController()?.nav(R.id.dataChoicesFragment, action)
                        },
                        learnMoreTechnicalData = { learnMoreTechnicalData(context) },
                        learnMoreDailyUsage = { learnMoreDailyUsage(context) },
                        learnMoreCrashReport = { learnMoreCrashReport(context) },
                        learnMoreMarketingData = { learnMoreMarketingData(context) },
                    )
                }
            }
        }
    }

    private fun updateMarketingDataChoice(newValue: Boolean, context: Context) {
        context.settings().isMarketingTelemetryEnabled = newValue
        if (context.settings().isMarketingTelemetryEnabled) {
            context.components.analytics.metrics.start(MetricServiceType.Marketing)
        } else {
            context.components.analytics.metrics.stop(MetricServiceType.Marketing)
        }
    }

    private fun updateTelemetryChoice(newValue: Boolean, context: Context) {
        context.settings().isTelemetryEnabled = newValue
        if (context.settings().isTelemetryEnabled) {
            context.components.analytics.metrics.start(MetricServiceType.Data)
            if (!context.settings().hasUserDisabledExperimentation) {
                context.settings().isExperimentationEnabled = true
            }
            requireComponents.nimbus.sdk.globalUserParticipation = true
            context.components.core.engine.notifyTelemetryPrefChanged(true)
        } else {
            context.components.analytics.metrics.stop(MetricServiceType.Data)
            context.settings().isExperimentationEnabled = false
            requireComponents.nimbus.sdk.globalUserParticipation = false
            context.components.core.engine.notifyTelemetryPrefChanged(false)
        }
        // Reset experiment identifiers on both opt-in and opt-out; it's likely
        // that in future we will need to pass in the new telemetry client_id
        // to this method when the user opts back in.
        context.components.nimbus.sdk.resetTelemetryIdentifiers()
    }

    private fun updateUsageChoice(newValue: Boolean, context: Context) {
        context.settings().isDailyUsagePingEnabled = newValue
        with(context.components.analytics.metrics) {
            if (context.settings().isDailyUsagePingEnabled) {
                start(MetricServiceType.UsageReporting)
            } else {
                stop(MetricServiceType.UsageReporting)
            }
        }
    }

    private suspend fun updateCrashChoice(newValue: CrashReportOption, context: Context) {
        context.settings().crashReportAlwaysSend = newValue == CrashReportOption.Auto
        context.settings().useNewCrashReporterDialog = newValue == CrashReportOption.Never
        crashReportCache.setReportOption(newValue)
    }

    private fun learnMoreTechnicalData(context: Context) {
        openLearnMoreUrlInSandboxedTab(
            context,
            SupportUtils.getSumoURLForTopic(
                context = context,
                topic = SupportUtils.SumoTopic.TECHNICAL_AND_INTERACTION_DATA,
            ),
        )
    }

    private fun learnMoreDailyUsage(context: Context) {
        openLearnMoreUrlInSandboxedTab(
            context,
            SupportUtils.getSumoURLForTopic(
                context = context,
                topic = SupportUtils.SumoTopic.USAGE_PING_SETTINGS,
            ),
        )
    }

    private fun learnMoreCrashReport(context: Context) {
        openLearnMoreUrlInSandboxedTab(
            context,
            SupportUtils.getSumoURLForTopic(
                context = context,
                topic = SupportUtils.SumoTopic.CRASH_REPORTS,
            ),
        )
    }

    private fun learnMoreMarketingData(context: Context) {
        openLearnMoreUrlInSandboxedTab(
            context,
            SupportUtils.getSumoURLForTopic(
                context = context,
                topic = SupportUtils.SumoTopic.MARKETING_DATA,
            ),
        )
    }

    private fun openLearnMoreUrlInSandboxedTab(context: Context, url: String) {
        SupportUtils.launchSandboxCustomTab(
            context = context,
            url = url,
        )
    }
}
