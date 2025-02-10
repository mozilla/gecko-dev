/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.AlertDialog
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.compose.base.button.TextButton
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.parseHtml
import org.mozilla.fenix.theme.FirefoxTheme

internal const val CLEAR_SITE_DATA_DIALOG_ROUTE = "clear_site_data_dialog"

/**
 * Clear Site Data Dialog.
 *
 * @param baseDomain The base domain of the current website.
 * @param onClearSiteDataClick Invoked when the user clicks on the "CLEAR" dialog button.
 * @param onCancelClick Invoked when the user clicks on the "CANCEL" dialog button.
 */
@Composable
fun ClearSiteDataDialog(
    baseDomain: String,
    onClearSiteDataClick: () -> Unit,
    onCancelClick: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = {},
        modifier = Modifier.background(
            color = FirefoxTheme.colors.layer2,
            shape = RoundedCornerShape(8.dp),
        ),
        title = {
            Text(
                text = stringResource(id = R.string.clear_site_data),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline7,
            )
        },
        text = {
            Text(
                text = parseHtml(stringResource(id = R.string.clear_site_data_dialog_description, baseDomain)),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.body2,
            )
        },
        confirmButton = {
            TextButton(
                text = stringResource(id = R.string.clear_site_data_dialog_positive_button_text),
                upperCaseText = false,
                onClick = { onClearSiteDataClick() },
            )
        },
        dismissButton = {
            TextButton(
                text = stringResource(id = R.string.clear_site_data_dialog_negative_button_text),
                upperCaseText = false,
                onClick = { onCancelClick() },
            )
        },
        backgroundColor = FirefoxTheme.colors.layer2,
    )
}

@Composable
@LightDarkPreview
private fun ClearSiteDataDialogPreview() {
    FirefoxTheme {
        ClearSiteDataDialog(
            baseDomain = "mozilla.org",
            onClearSiteDataClick = {},
            onCancelClick = {},
        )
    }
}
