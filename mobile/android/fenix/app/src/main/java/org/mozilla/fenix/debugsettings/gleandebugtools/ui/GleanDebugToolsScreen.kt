/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.gleandebugtools.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Text
import androidx.compose.material.TextField
import androidx.compose.material.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsAction
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsState
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsStore
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Glean Debug Tools UI that allows for glean test pings to be sent.
 */
@Composable
fun GleanDebugToolsScreen(
    gleanDebugToolsStore: GleanDebugToolsStore,
) {
    val gleanDebugToolsState by gleanDebugToolsStore.observeAsState(gleanDebugToolsStore.state) { it }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(top = FirefoxTheme.space.small),
    ) {
        GleanDebugLoggingSection(logPingsToConsoleEnabled = gleanDebugToolsState.logPingsToConsoleEnabled) {
            gleanDebugToolsStore.dispatch(GleanDebugToolsAction.LogPingsToConsoleToggled)
        }

        Spacer(modifier = Modifier.height(FirefoxTheme.space.xSmall))

        GleanDebugViewSection(
            buttonsEnabled = gleanDebugToolsState.isDebugTagButtonEnabled,
            debugViewTag = gleanDebugToolsState.debugViewTag,
            hasDebugViewTagError = gleanDebugToolsStore.state.hasDebugViewTagError,
            onOpenDebugView = { useDebugViewTag ->
                gleanDebugToolsStore.dispatch(
                    GleanDebugToolsAction.OpenDebugView(
                        useDebugViewTag = useDebugViewTag,
                    ),
                )
            },
            onCopyDebugViewLink = { useDebugViewTag ->
                gleanDebugToolsStore.dispatch(
                    GleanDebugToolsAction.CopyDebugViewLink(
                        useDebugViewTag = useDebugViewTag,
                    ),
                )
            },
        ) { newTag ->
            gleanDebugToolsStore.dispatch(GleanDebugToolsAction.DebugViewTagChanged(newTag))
        }

        Spacer(modifier = Modifier.height(FirefoxTheme.space.xSmall))

        GleanDebugSendPingsSection(
            isButtonEnabled = gleanDebugToolsState.isDebugTagButtonEnabled,
            onSendPendingEventPing = { gleanDebugToolsStore.dispatch(GleanDebugToolsAction.SendPendingEventPing) },
            onSendBaselinePing = { gleanDebugToolsStore.dispatch(GleanDebugToolsAction.SendBaselinePing) },
            onSendMetricsPing = { gleanDebugToolsStore.dispatch(GleanDebugToolsAction.SendMetricsPing) },
        )
    }
}

@Composable
private fun GleanDebugLoggingSection(
    logPingsToConsoleEnabled: Boolean,
    onLogPingsToConsoleToggled: () -> Unit,
) {
    GleanDebugSectionTitle(text = stringResource(R.string.glean_debug_tools_logging_title))

    SwitchWithLabel(
        label = stringResource(R.string.glean_debug_tools_log_pings_to_console),
        checked = logPingsToConsoleEnabled,
        modifier = Modifier.padding(horizontal = FirefoxTheme.space.small),
    ) {
        onLogPingsToConsoleToggled()
    }
}

@Composable
@Suppress("LongMethod")
private fun GleanDebugViewSection(
    buttonsEnabled: Boolean,
    debugViewTag: String,
    hasDebugViewTagError: Boolean,
    onOpenDebugView: (Boolean) -> Unit,
    onCopyDebugViewLink: (Boolean) -> Unit,
    onDebugViewTagChanged: (String) -> Unit,
) {
    val keyboardController = LocalSoftwareKeyboardController.current

    GleanDebugSectionTitle(text = stringResource(id = R.string.glean_debug_tools_debug_view_title))

    TextField(
        value = debugViewTag,
        onValueChange = {
            if (it.all { char -> char.isLetterOrDigit() || char == '-' }) {
                onDebugViewTagChanged(it)
            }
        },
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = FirefoxTheme.space.small),
        textStyle = FirefoxTheme.typography.subtitle1,
        isError = hasDebugViewTagError,
        placeholder = {
            Text(
                text = stringResource(R.string.glean_debug_tools_debug_view_tag_placeholder),
                color = FirefoxTheme.colors.textSecondary,
            )
        },
        keyboardOptions = KeyboardOptions(
            keyboardType = KeyboardType.Ascii,
        ),
        keyboardActions = KeyboardActions(
            onDone = {
                keyboardController?.hide()
            },
        ),
        colors = TextFieldDefaults.textFieldColors(
            textColor = FirefoxTheme.colors.textPrimary,
            backgroundColor = Color.Transparent,
            cursorColor = FirefoxTheme.colors.borderFormDefault,
            errorCursorColor = FirefoxTheme.colors.borderCritical,
            focusedIndicatorColor = FirefoxTheme.colors.borderPrimary,
            unfocusedIndicatorColor = FirefoxTheme.colors.borderPrimary,
            errorIndicatorColor = FirefoxTheme.colors.borderCritical,
        ),
    )

    if (hasDebugViewTagError) {
        Text(
            text = stringResource(
                R.string.glean_debug_tools_debug_view_tag_error,
                GleanDebugToolsState.DEBUG_VIEW_TAG_MAX_LENGTH,
            ),
            modifier = Modifier.padding(start = FirefoxTheme.space.small),
            color = FirefoxTheme.colors.textCritical,
            style = FirefoxTheme.typography.caption,
        )
    }

    if (buttonsEnabled) {
        GleanDebugButton(
            text = stringResource(
                R.string.glean_debug_tools_open_debug_view_debug_view_tag,
                debugViewTag,
            ),
        ) {
            onOpenDebugView(true)
        }

        GleanDebugButton(
            text = stringResource(
                R.string.glean_debug_tools_copy_debug_view_link_debug_view_tag,
                debugViewTag,
            ),
        ) { onCopyDebugViewLink(true) }
    }

    GleanDebugButton(text = stringResource(R.string.glean_debug_tools_open_debug_view)) {
        onOpenDebugView(false)
    }

    GleanDebugButton(text = stringResource(R.string.glean_debug_tools_copy_debug_view_link)) {
        onCopyDebugViewLink(false)
    }
}

@Composable
private fun GleanDebugSendPingsSection(
    isButtonEnabled: Boolean,
    onSendPendingEventPing: () -> Unit,
    onSendBaselinePing: () -> Unit,
    onSendMetricsPing: () -> Unit,
) {
    Column(
        modifier = Modifier.padding(horizontal = FirefoxTheme.space.small),
    ) {
        GleanTestPingButton(
            text = stringResource(R.string.glean_debug_tools_send_pending_event_pings_button_text),
            enabled = isButtonEnabled,
            onClick = onSendPendingEventPing,
        )

        GleanTestPingButton(
            text = stringResource(R.string.glean_debug_tools_send_baseline_pings_button_text),
            enabled = isButtonEnabled,
            onClick = onSendBaselinePing,
        )

        GleanTestPingButton(
            text = stringResource(R.string.glean_debug_tools_send_metrics_pings_button_text),
            enabled = isButtonEnabled,
            onClick = onSendMetricsPing,
        )
    }
}

/**
 * The UI for a section title on the Glean Debug Tools page.
 *
 * @param text The text for a section of Glean Debug Tools page.
 */
@Composable
private fun GleanDebugSectionTitle(
    text: String,
) {
    Text(
        text = text,
        modifier = Modifier.padding(horizontal = FirefoxTheme.space.small),
        color = FirefoxTheme.colors.textAccent,
        style = FirefoxTheme.typography.subtitle1,
    )
}

/**
 * The UI for a button on the Glean Debug Tools page.
 *
 * @param text The text on the button.
 * @param onClick Called when the user clicks on the button.
 */
@Composable
private fun GleanDebugButton(
    text: String,
    onClick: () -> Unit,
) {
    TextListItem(
        label = text,
        modifier = Modifier.fillMaxWidth(),
        maxLabelLines = Int.MAX_VALUE,
        onClick = onClick,
    )
}

/**
 * The UI for a send test ping button on the Glean Debug Tools page.
 *
 * @param text The text on the button.
 * @param enabled Controls the enabled state of the list item. When `false`, the list item will not
 * be clickable.
 * @param onClick Called when the user clicks on the button.
 */
@Composable
private fun GleanTestPingButton(
    text: String,
    enabled: Boolean,
    onClick: () -> Unit,
) {
    PrimaryButton(
        text = text,
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = FirefoxTheme.space.xxSmall),
        enabled = enabled,
        onClick = onClick,
    )
}

@Composable
@FlexibleWindowLightDarkPreview
private fun GleanDebugToolsPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(
                color = FirefoxTheme.colors.layer1,
            ),
        ) {
            GleanDebugToolsScreen(gleanDebugToolsStore = GleanDebugToolsStore())
        }
    }
}
