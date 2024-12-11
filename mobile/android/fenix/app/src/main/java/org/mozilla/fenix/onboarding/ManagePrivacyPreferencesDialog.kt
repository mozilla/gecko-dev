/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesAction
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesState
import org.mozilla.fenix.onboarding.store.PrivacyPreferencesStore
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Dialog to manage privacy preferences during onboarding.
 */
@Composable
fun ManagePrivacyPreferencesDialog(
    store: PrivacyPreferencesStore,
    onDismissRequest: () -> Unit,
    onCrashReportingLinkClick: () -> Unit,
    onUsageDataLinkClick: () -> Unit,
) {
    val state by store.observeAsState(initialValue = store.state) { state -> state }

    Dialog(
        onDismissRequest = { onDismissRequest() },
        properties = DialogProperties(dismissOnClickOutside = false),
    ) {
        Card(
            backgroundColor = FirefoxTheme.colors.layer2,
            shape = RoundedCornerShape(8.dp),
        ) {
            Column(Modifier.padding(16.dp)) {
                Title()

                Spacer(modifier = Modifier.height(16.dp))

                UsageDataPreference(store, state.usageDataChecked, onUsageDataLinkClick)

                Spacer(modifier = Modifier.height(24.dp))

                CrashReportingPreference(
                    store,
                    state.crashReportingChecked,
                    onCrashReportingLinkClick,
                )

                Spacer(modifier = Modifier.height(16.dp))

                Buttons(store, state, onDismissRequest)
            }
        }
    }
}

@Composable
private fun Title() {
    Text(
        text = stringResource(R.string.onboarding_preferences_dialog_title),
        color = FirefoxTheme.colors.textPrimary,
        style = FirefoxTheme.typography.headline7,
        maxLines = 1,
    )
}

@Composable
private fun CrashReportingPreference(
    store: PrivacyPreferencesStore,
    crashReportingEnabled: Boolean,
    onLinkClick: () -> Unit,
) {
    SwitchWithLabel(
        label = stringResource(R.string.onboarding_preferences_dialog_crash_reporting_title),
        checked = crashReportingEnabled,
        onCheckedChange = { store.dispatch(PrivacyPreferencesAction.CrashReportingChecked(it)) },
        modifier = Modifier.wrapContentWidth(),
    )

    Spacer(modifier = Modifier.height(8.dp))

    Text(
        text = stringResource(R.string.onboarding_preferences_dialog_crash_reporting_description),
        color = FirefoxTheme.colors.textPrimary,
        style = FirefoxTheme.typography.caption,
    )

    Spacer(modifier = Modifier.height(8.dp))

    LinkText(
        text = stringResource(R.string.onboarding_preferences_dialog_crash_reporting_learn_more),
        linkTextStates = listOf(
            LinkTextState(
                text = stringResource(id = R.string.onboarding_preferences_dialog_crash_reporting_learn_more),
                url = "",
                onClick = { onLinkClick() },
            ),
        ),
        style = FirefoxTheme.typography.caption.copy(color = FirefoxTheme.colors.textPrimary),
    )
}

@Composable
private fun UsageDataPreference(
    store: PrivacyPreferencesStore,
    usageDataEnabled: Boolean,
    onLinkClick: () -> Unit,
) {
    SwitchWithLabel(
        label = stringResource(R.string.onboarding_preferences_dialog_usage_data_title),
        checked = usageDataEnabled,
        onCheckedChange = { store.dispatch(PrivacyPreferencesAction.UsageDataUserChecked(it)) },
        modifier = Modifier.wrapContentWidth(),
    )

    Spacer(modifier = Modifier.height(8.dp))

    Text(
        text = stringResource(R.string.onboarding_preferences_dialog_usage_data_description),
        color = FirefoxTheme.colors.textPrimary,
        style = FirefoxTheme.typography.caption,
    )

    Spacer(modifier = Modifier.height(8.dp))

    LinkText(
        text = stringResource(R.string.onboarding_preferences_dialog_usage_data_learn_more),
        linkTextStates = listOf(
            LinkTextState(
                text = stringResource(id = R.string.onboarding_preferences_dialog_usage_data_learn_more),
                url = "",
                onClick = { onLinkClick() },
            ),
        ),
        style = FirefoxTheme.typography.caption.copy(color = FirefoxTheme.colors.textPrimary),
    )
}

@Composable
private fun Buttons(
    store: PrivacyPreferencesStore,
    state: PrivacyPreferencesState,
    onDismissRequest: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .wrapContentHeight(),
        horizontalArrangement = Arrangement.End,
    ) {
        NegativeButton(onDismissRequest)

        PositiveButton(state, store, onDismissRequest)
    }
}

@Composable
private fun NegativeButton(onDismissRequest: () -> Unit) {
    DialogButton(
        text = stringResource(R.string.onboarding_preferences_dialog_negative_button),
        onClick = { onDismissRequest() },
    )
}

@Composable
private fun PositiveButton(
    state: PrivacyPreferencesState,
    store: PrivacyPreferencesStore,
    onDismissRequest: () -> Unit,
) {
    DialogButton(
        text = stringResource(R.string.onboarding_preferences_dialog_positive_button),
        onClick = {
            if (crashReportingPreferenceChanged(state)) {
                store.dispatch(
                    PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo(state.crashReportingChecked),
                )
            }

            if (usageDataPreferenceChanged(state)) {
                store.dispatch(
                    PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo(state.usageDataChecked),
                )
            }

            onDismissRequest()
        },
    )
}

private fun crashReportingPreferenceChanged(state: PrivacyPreferencesState) =
    state.crashReportingEnabled != state.crashReportingChecked

private fun usageDataPreferenceChanged(state: PrivacyPreferencesState) =
    state.usageDataEnabled != state.usageDataChecked

@Composable
private fun DialogButton(text: String, onClick: () -> Unit) {
    TextButton(onClick = onClick) {
        Text(
            text.uppercase(),
            color = FirefoxTheme.colors.textAccent,
            style = FirefoxTheme.typography.button,
            maxLines = 1,
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun ManagePrivacyPreferencesDialogPreview() {
    FirefoxTheme {
        ManagePrivacyPreferencesDialog(PrivacyPreferencesStore(), {}, {}, {})
    }
}
