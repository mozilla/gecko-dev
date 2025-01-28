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
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Dropdown
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.compose.menu.MenuItem
import org.mozilla.fenix.compose.text.Text
import org.mozilla.fenix.compose.textfield.TextField
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsAction
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsState
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsStore
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Glean Debug Tools UI that allows for glean test pings to be sent.
 *
 * @param gleanDebugToolsStore [GleanDebugToolsStore] used to access [GleanDebugToolsState].
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
            .padding(top = FirefoxTheme.layout.space.dynamic400),
    ) {
        GleanDebugLoggingSection(logPingsToConsoleEnabled = gleanDebugToolsState.logPingsToConsoleEnabled) {
            gleanDebugToolsStore.dispatch(GleanDebugToolsAction.LogPingsToConsoleToggled)
        }

        Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.dynamic150))

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

        Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.dynamic150))

        GleanDebugSendPingsSection(
            isButtonEnabled = gleanDebugToolsState.isDebugTagButtonEnabled,
            curPing = gleanDebugToolsState.pingType,
            pingTypes = gleanDebugToolsState.pingTypes,
            onPingItemClicked = { gleanDebugToolsStore.dispatch(GleanDebugToolsAction.ChangePingType(it)) },
            onSendPing = { gleanDebugToolsStore.dispatch(GleanDebugToolsAction.SendPing) },
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
        modifier = Modifier.padding(horizontal = FirefoxTheme.layout.space.dynamic400),
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

    Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.dynamic400))

    TextField(
        value = debugViewTag,
        onValueChange = {
            if (it.all { char -> char.isLetterOrDigit() || char == '-' }) {
                onDebugViewTagChanged(it)
            }
        },
        placeholder = stringResource(R.string.glean_debug_tools_debug_view_tag_placeholder),
        errorText = stringResource(
            R.string.glean_debug_tools_debug_view_tag_error,
            GleanDebugToolsState.DEBUG_VIEW_TAG_MAX_LENGTH,
        ),
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = FirefoxTheme.layout.space.dynamic400),
        isError = hasDebugViewTagError,
        keyboardOptions = KeyboardOptions(
            keyboardType = KeyboardType.Ascii,
        ),
        keyboardActions = KeyboardActions(
            onDone = {
                keyboardController?.hide()
            },
        ),
    )

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
    curPing: String,
    pingTypes: List<String>,
    onPingItemClicked: (String) -> Unit,
    onSendPing: () -> Unit,
) {
    Column(
        modifier = Modifier.padding(horizontal = FirefoxTheme.layout.space.dynamic400),
    ) {
        Dropdown(
            label = "Ping Type",
            placeholder = "",
            dropdownItems = getPingDropdownMenu(
                curPing = curPing,
                pings = pingTypes,
                onClickItem = onPingItemClicked,
            ),
        )

        Spacer(modifier = Modifier.height(FirefoxTheme.layout.space.dynamic400))

        PrimaryButton(
            text = stringResource(R.string.glean_debug_tools_send_ping_button_text),
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = FirefoxTheme.layout.space.dynamic200),
            enabled = isButtonEnabled,
            onClick = onSendPing,
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
        modifier = Modifier.padding(horizontal = FirefoxTheme.layout.space.dynamic400),
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

private fun getPingDropdownMenu(
    curPing: String,
    pings: List<String>,
    onClickItem: (String) -> Unit,
) = pings.map {
    MenuItem.CheckableItem(
        text = Text.String(it),
        isChecked = it == curPing,
    ) { onClickItem(it) }
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
            GleanDebugToolsScreen(
                gleanDebugToolsStore = GleanDebugToolsStore(
                    initialState = GleanDebugToolsState(
                        logPingsToConsoleEnabled = false,
                        debugViewTag = "",
                    ),
                ),
            )
        }
    }
}
