/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import androidx.compose.foundation.background
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.AlertDialog
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.theme.FirefoxTheme
import java.util.Locale

/**
 * Cancel Download Languages file Dialog.
 *
 * @param language Language name that should be displayed in the dialogue title.
 * @param onConfirmDelete Invoked when the user clicks on the "Yes" dialog button.
 * @param onCancel Invoked when the user clicks on the "No" dialog button.
 */
@Composable
fun CancelDownloadFileDialog(
    language: String? = null,
    onConfirmDelete: () -> Unit,
    onCancel: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = {},
        modifier = Modifier.background(
            color = FirefoxTheme.colors.layer2,
            shape = RoundedCornerShape(8.dp),
        ),
        title = {
            language?.let {
                Text(
                    text = stringResource(
                        id = R.string.cancel_download_language_file_dialog_title,
                        it,
                    ),
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline7,
                )
            }
        },
        confirmButton = {
            TextButton(
                text = stringResource(id = R.string.cancel_download_language_file_dialog_positive_button_text),
                upperCaseText = false,
                onClick = { onConfirmDelete() },
            )
        },
        dismissButton = {
            TextButton(
                text = stringResource(id = R.string.cancel_download_language_file_negative_button_text),
                upperCaseText = false,
                onClick = { onCancel() },
            )
        },
        backgroundColor = FirefoxTheme.colors.layer2,
    )
}

@Composable
@LightDarkPreview
private fun CancelDownloadFileDialogPreview() {
    FirefoxTheme {
        CancelDownloadFileDialog(
            language = Locale.CHINA.displayLanguage,
            onConfirmDelete = {},
            onCancel = {},
        )
    }
}
