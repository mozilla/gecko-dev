/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Checkbox
import androidx.compose.material.CheckboxDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import androidx.fragment.app.DialogFragment
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.store.CrashAction
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Dialog to request whether a user wants to submit crashes that have not been reported.
 *
 * @param dispatcher Callback to dispatch various [CrashAction]s in response to user input.
 * @param crashIDs If present holds the list of minidump files requested over Remote Settings.
 * @param localContext Application context to provide for Learn More links opening.
 */
class UnsubmittedCrashDialog(
    private val dispatcher: (action: CrashAction) -> Unit,
    private val crashIDs: Array<String>?,
    private val localContext: Context,
) : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return activity?.let { activity ->
            AlertDialog.Builder(activity)
                .setView(
                    ComposeView(activity).apply {
                        setContent {
                            FirefoxTheme {
                                CrashCard(
                                    dismiss = ::dismiss,
                                    dispatcher = dispatcher,
                                    crashIDs = crashIDs,
                                    cardContext = localContext,
                                )
                            }
                        }
                    },
                )
                .create()
        } ?: throw IllegalStateException("Activity cannot be null")
    }

    companion object {
        const val TAG = "unsubmitted crash dialog tag"
    }
}

@Suppress("LongMethod")
@Composable
private fun CrashCard(
    dismiss: () -> Unit,
    dispatcher: (action: CrashAction) -> Unit,
    crashIDs: Array<String>?,
    cardContext: Context?,
) {
    var requestedByDevs = if (crashIDs != null && crashIDs.size > 0) {
        true
    } else {
        false
    }

    var msg = if (requestedByDevs) {
        if (crashIDs?.size == 1) {
            stringResource(
                R.string.unsubmitted_crash_requested_by_devs_dialog_title,
                stringResource(R.string.app_name),
            )
        } else {
            stringResource(
                R.string.unsubmitted_crashes_requested_by_devs_dialog_title,
                crashIDs!!.size,
                stringResource(R.string.app_name),
            )
        }
    } else {
        stringResource(
            R.string.unsubmitted_crash_dialog_title,
            stringResource(R.string.app_name),
        )
    }

    var checkboxChecked by remember { mutableStateOf(false) }
    Column(modifier = Modifier.padding(16.dp)) {
        Text(
            text = msg,
            modifier = Modifier
                .semantics { heading() },
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline5,
        )

        Spacer(modifier = Modifier.height(16.dp))

        Row(verticalAlignment = Alignment.CenterVertically) {
            if (!requestedByDevs) {
                Checkbox(
                    checked = checkboxChecked,
                    colors = CheckboxDefaults.colors(
                        checkedColor = FirefoxTheme.colors.formSelected,
                        uncheckedColor = FirefoxTheme.colors.formDefault,
                    ),
                    onCheckedChange = { checkboxChecked = it },
                )
                Text(
                    text = stringResource(R.string.unsubmitted_crash_dialog_checkbox_label),
                    color = FirefoxTheme.colors.textSecondary,
                )
            }
        }

        if (requestedByDevs) {
            Spacer(modifier = Modifier.height(16.dp))
            Row(
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(
                    text = stringResource(R.string.unsubmitted_crash_requested_by_devs_learn_more).uppercase(),
                    color = FirefoxTheme.colors.actionPrimary,
                    modifier = Modifier.clickable {
                        if (cardContext != null) {
                            CoroutineScope(Dispatchers.Main).launch {
                                val url = SupportUtils.getGenericSumoURLForTopic(
                                    topic = SupportUtils.SumoTopic.REQUESTED_CRASH_MINIDUMP,
                                )
                                val intent = SupportUtils.createCustomTabIntent(cardContext, url)
                                cardContext.startActivity(intent)
                            }
                        }
                    },
                )
                Text(
                    text = stringResource(R.string.unsubmitted_crash_requested_by_devs_dialog_never_button).uppercase(),
                    color = FirefoxTheme.colors.textSecondary,
                    modifier = Modifier.clickable {
                        dispatcher(CrashAction.CancelForEverTapped)
                        dismiss()
                    },
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        Row(
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(
                text = stringResource(R.string.unsubmitted_crash_dialog_negative_button).uppercase(),
                color = FirefoxTheme.colors.textSecondary,
                modifier = Modifier.clickable {
                    dispatcher(CrashAction.CancelTapped)
                    dismiss()
                },
            )
            Text(
                text = stringResource(R.string.unsubmitted_crash_dialog_positive_button).uppercase(),
                color = FirefoxTheme.colors.textSecondary,
                modifier = Modifier.clickable {
                    dispatcher(CrashAction.ReportTapped(!requestedByDevs && checkboxChecked, crashIDs))
                    dismiss()
                },
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun CrashDialogPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            CrashCard(
                dismiss = {},
                dispatcher = {},
                crashIDs = null,
                cardContext = null,
            )
        }
    }
}
