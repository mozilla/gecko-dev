/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.lib.crash.store.CrashReportOption
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.list.RadioButtonListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Holds the state and callbacks for the Data Choices settings screen.
 *
 * This class encapsulates both the current user preferences and the action handlers
 * required to handle interactions or changes to preferences.
 *
 * @property telemetryEnabled Whether telemetry data collection is currently enabled.
 * @property usagePingEnabled Whether the daily usage ping is currently enabled.
 * @property studiesEnabled Whether participation in studies is currently enabled.
 * @property measurementDataEnabled Whether participation in marketing data is currently enabled.
 * @property selectedCrashOption The currently selected crash reporting option.
 */
data class DataChoicesParams(
    val telemetryEnabled: Boolean,
    val usagePingEnabled: Boolean,
    val studiesEnabled: Boolean,
    val measurementDataEnabled: Boolean,
    val selectedCrashOption: CrashReportOption,
)

/**
 * Composable function that renders the Data Choices settings screen.
 *
 * This screen allows the user to view and modify their preferences related to telemetry,
 * crash reporting, usage data, and participation in studies.
 *
 * @param params The [DataChoicesParams] containing current preference values and
 * callbacks to handle user interaction.
 * @param onTelemetryToggle Callback when the telemetry toggle is changed.
 * @param onUsagePingToggle Callback when the usage ping toggle is changed.
 * @param onMarketingDataToggled Callback when the marketing data toggle is changed.
 * @param onCrashOptionSelected Callback when the crash reporting option is selected.
 * @param onStudiesClick Callback when the studies section is clicked.
 * @param learnMoreTechnicalData Callback to handle "Learn More" about telemetry.
 * @param learnMoreDailyUsage Callback to handle "Learn More" about daily usage ping.
 * @param learnMoreCrashReport Callback to handle "Learn More" about crash reporting.
 * @param learnMoreMarketingData Callback to handle "Learn More" about marketing data.
 **/
@Suppress("LongParameterList")
@Composable
internal fun DataChoicesScreen(
    params: DataChoicesParams,
    onTelemetryToggle: (Boolean) -> Unit,
    onUsagePingToggle: (Boolean) -> Unit,
    onMarketingDataToggled: (Boolean) -> Unit,
    onCrashOptionSelected: (CrashReportOption) -> Unit,
    onStudiesClick: () -> Unit,
    learnMoreTechnicalData: () -> Unit,
    learnMoreDailyUsage: () -> Unit,
    learnMoreCrashReport: () -> Unit,
    learnMoreMarketingData: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer1)
            .verticalScroll(rememberScrollState())
            .padding(top = 10.dp, bottom = 38.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        // Technical Data Section
        TogglePreferenceSection(
            categoryTitle = stringResource(R.string.technical_data_category),
            preferenceTitle = stringResource(R.string.preference_usage_data_2),
            preferenceSummary = stringResource(R.string.preferences_usage_data_description_1),
            learnMoreText = stringResource(R.string.preference_usage_data_learn_more_2),
            isToggled = params.telemetryEnabled,
            onToggleChanged = onTelemetryToggle,
            onLearnMoreClicked = learnMoreTechnicalData,
        )

        Divider()

        StudiesSection(
            studiesEnabled = params.studiesEnabled,
            sectionEnabled = params.telemetryEnabled,
            onClick = onStudiesClick,
        )

        Divider()

        // Usage Data Section
        TogglePreferenceSection(
            categoryTitle = stringResource(R.string.usage_data_category),
            preferenceTitle = stringResource(R.string.preferences_daily_usage_ping_title),
            preferenceSummary = stringResource(R.string.preferences_daily_usage_ping_description),
            learnMoreText = stringResource(R.string.preferences_daily_usage_ping_learn_more),
            isToggled = params.usagePingEnabled,
            onToggleChanged = onUsagePingToggle,
            onLearnMoreClicked = learnMoreDailyUsage,
        )

        Divider()

        // Crash reports section
        CrashReportsSection(
            learnMoreText = stringResource(R.string.preferences_crashes_learn_more),
            selectedOption = params.selectedCrashOption,
            onOptionSelected = onCrashOptionSelected,
            onLearnMoreClicked = learnMoreCrashReport,
        )

        Divider()

        // Campaign measurement Section
        TogglePreferenceSection(
            categoryTitle = stringResource(R.string.preferences_marketing_data_title),
            preferenceTitle = stringResource(R.string.preferences_marketing_data_2),
            preferenceSummary = stringResource(R.string.preferences_marketing_data_description_4),
            learnMoreText = stringResource(R.string.preferences_marketing_data_learn_more),
            isToggled = params.measurementDataEnabled,
            onToggleChanged = onMarketingDataToggled,
            onLearnMoreClicked = learnMoreMarketingData,
        )
    }
}

/**
 * Composable section for configuring crash reporting preferences.
 *
 * @param learnMoreText The text to display for the "Learn More" link.
 * @param selectedOption The currently selected crash reporting option.
 * @param onOptionSelected Callback invoked when the user selects a different crash report option.
 * @param onLearnMoreClicked Callback invoked when the "Learn More" link is clicked.
 * */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
private fun CrashReportsSection(
    learnMoreText: String,
    selectedOption: CrashReportOption = CrashReportOption.Ask,
    onOptionSelected: (CrashReportOption) -> Unit,
    onLearnMoreClicked: () -> Unit,
    ) {
    Column(
        verticalArrangement = Arrangement.spacedBy(16.dp),
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        TitleText(
            text = stringResource(R.string.crash_reports_data_category),
            modifier = Modifier
                .padding(horizontal = 16.dp),
            )

        SectionBodyText(
            stringResource(R.string.crash_reporting_description),
            Modifier
                .padding(horizontal = 16.dp),
            )

        Column(
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            CrashReportOption.entries.forEach { crashReportOption ->
                RadioButtonListItem(
                    label = stringResource(crashReportOption.labelId),
                    selected = selectedOption == crashReportOption,
                    modifier = Modifier
                        .semantics {
                            testTag = "data.collection.$crashReportOption.radio.button"
                            testTagsAsResourceId = true
                        },
                    maxLabelLines = 1,
                    description = null,
                    maxDescriptionLines = 1,
                    onClick = { onOptionSelected(crashReportOption) },
                )
            }
        }
        LearnMoreLink(onLearnMoreClicked, learnMoreText)
    }
}

@Composable
private fun TitleText(text: String, modifier: Modifier) {
    Text(
        text = text,
        style = FirefoxTheme.typography.body2,
        color = FirefoxTheme.colors.textAccent,
        modifier = modifier,
    )
}

@Composable
private fun SectionBodyText(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        style = FirefoxTheme.typography.body2,
        color = FirefoxTheme.colors.textSecondary,
        modifier = modifier,
    )
}

/**
 * Composable section that displays a toggleable user preference with a title, summary,
 * and an optional "Learn More" link.
 *
 * @param categoryTitle The title of the category this preference belongs to (usually shown above the preference).
 * @param preferenceTitle The title of the individual preference.
 * @param preferenceSummary A brief description explaining what the preference does.
 * @param learnMoreText The text shown for the "Learn More" link.
 * @param isToggled Whether the preference toggle is currently enabled (on) or disabled (off).
 * @param onToggleChanged Callback invoked when the toggle state changes.
 * @param onLearnMoreClicked Callback invoked when the "Learn More" link is clicked.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
private fun TogglePreferenceSection(
    categoryTitle: String,
    preferenceTitle: String,
    preferenceSummary: String,
    learnMoreText: String,
    isToggled: Boolean,
    onToggleChanged: (Boolean) -> Unit,
    onLearnMoreClicked: () -> Unit,
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(16.dp),
        modifier = Modifier
            .fillMaxWidth()
            .background(FirefoxTheme.colors.layer1),
    ) {
        TitleText(categoryTitle, Modifier.padding(horizontal = 16.dp))

        // Section Body
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .clickable(
                    onClick = { onToggleChanged(!isToggled) },
                )
                .padding(horizontal = 16.dp),
        ) {
            Column(
                modifier = Modifier
                    .weight(1f),
            ) {
                Text(
                    text = preferenceTitle,
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.subtitle1,
                )
                Spacer(modifier = Modifier.height(4.dp))
                SectionBodyText(preferenceSummary)
            }

            Switch(
                checked = isToggled,
                onCheckedChange = {},
                colors = SwitchDefaults.colors(
                    checkedThumbColor = FirefoxTheme.colors.formOn,
                    checkedTrackColor = FirefoxTheme.colors.formSurface,
                    uncheckedThumbColor = FirefoxTheme.colors.formOff,
                    uncheckedTrackColor = FirefoxTheme.colors.formSurface,
                ),
                modifier = Modifier
                    .semantics {
                        testTag = "data.collection.$preferenceTitle.toggle"
                        testTagsAsResourceId = true
                    },
            )
        }
        LearnMoreLink(onLearnMoreClicked, learnMoreText)
    }
}

/**
 * Composable section that displays the user’s participation status in studies or experiments.
 *
 * @param studiesEnabled Whether the user is currently enrolled in studies.
 *                       Affects the summary text shown in the section.
 * @param sectionEnabled Whether the section is interactive. If false, the section is visually disabled
 *                       and does not respond to clicks.
 * @param onClick Callback invoked when the section is clicked (if enabled).
 */
@Composable
private fun StudiesSection(
    studiesEnabled: Boolean = true,
    sectionEnabled: Boolean = true,
    onClick: () -> Unit,
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(16.dp),
        modifier = Modifier
            .fillMaxWidth()
            .background(FirefoxTheme.colors.layer1),
    ) {
        TitleText(
            stringResource(R.string.studies_data_category),
            Modifier.padding(horizontal = 16.dp),
        )

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .then(if (sectionEnabled) Modifier.clickable(onClick = onClick) else Modifier)
                .padding(horizontal = 16.dp),
        ) {
            Text(
                text = stringResource(R.string.studies_title),
                style = FirefoxTheme.typography.subtitle1,
                color = if (sectionEnabled) { FirefoxTheme.colors.textPrimary } else {
                    FirefoxTheme.colors.textDisabled
                },
            )
            Text(
                text = stringResource(if (studiesEnabled) R.string.studies_on else R.string.studies_off),
                style = FirefoxTheme.typography.body2,
                color = if (sectionEnabled) { FirefoxTheme.colors.textSecondary } else {
                    FirefoxTheme.colors.textDisabled
                },
            )
        }
    }
}

/**
 * Composable that displays a "Learn More" text link.
 *
 * @param onLearnMoreClicked Callback invoked when the user clicks the link.
 * @param learnMoreText The text to display as the link.
 */
@Composable
private fun LearnMoreLink(onLearnMoreClicked: () -> Unit, learnMoreText: String) {
    val learnMoreState = LinkTextState(
        text = learnMoreText,
        url = "",
        onClick = {
            onLearnMoreClicked()
        },
    )
    Column(
        modifier = Modifier
            .clickable(onClick = { onLearnMoreClicked() })
            .fillMaxWidth()
            .padding(horizontal = 16.dp)
            .padding(top = 16.dp),
    ) {
        LinkText(
            text = learnMoreText,
            linkTextStates = listOf(learnMoreState),
            style = FirefoxTheme.typography.subtitle1.copy(
                color = FirefoxTheme.colors.textPrimary,
                textDecoration = TextDecoration.Underline,
            ),
            linkTextColor = FirefoxTheme.colors.textPrimary,
            linkTextDecoration = TextDecoration.Underline,
            textAlign = TextAlign.Center,
            shouldApplyAccessibleSize = false,
        )
    }
}

/**
 * Provides preview data for the [DataChoicesScreen] Composable.
 *
 * This class implements [PreviewParameterProvider] and supplies multiple
 * [DataChoicesParams] configurations to preview different UI states.
 */
class DataChoicesPreviewProvider : PreviewParameterProvider<DataChoicesParams> {
    override val values = sequenceOf(
        DataChoicesParams(
            telemetryEnabled = false,
            usagePingEnabled = true,
            studiesEnabled = false,
            selectedCrashOption = CrashReportOption.Ask,
            measurementDataEnabled = true,
        ),
        DataChoicesParams(
            telemetryEnabled = true,
            usagePingEnabled = false,
            studiesEnabled = true,
            selectedCrashOption = CrashReportOption.Auto,
            measurementDataEnabled = false,
        ),
    )
}

@FlexibleWindowLightDarkPreview
@Composable
private fun DataChoicesPreview(
    @PreviewParameter(DataChoicesPreviewProvider::class)
    params: DataChoicesParams,
) {
    FirefoxTheme {
        DataChoicesScreen(
            params,
            onTelemetryToggle = { },
            onUsagePingToggle = { },
            onCrashOptionSelected = { },
            onStudiesClick = { },
            learnMoreTechnicalData = { },
            learnMoreDailyUsage = { },
            learnMoreCrashReport = { },
            onMarketingDataToggled = { },
            learnMoreMarketingData = { },
        )
    }
}
