/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewScreenSizes
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The header UI used for microsurvey.
 *
 * @param title The text that will be visible on the header.
 * @param onCloseButtonClick Invoked when the close button is clicked.
 */
@Composable
fun MicrosurveyHeader(
    title: String,
    onCloseButtonClick: () -> Unit,
) {
    Row(
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
    ) {
        Row(
            modifier = Modifier
                .wrapContentWidth()
                .weight(1f, fill = false)
                .padding(start = 32.dp),
        ) {
            Image(
                painter = painterResource(R.drawable.ic_firefox),
                contentDescription = stringResource(id = R.string.microsurvey_app_icon_content_description),
                modifier = Modifier.size(24.dp),
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = title,
                style = FirefoxTheme.typography.headline6,
                color = FirefoxTheme.colors.textPrimary,
            )
        }
        IconButton(onClick = onCloseButtonClick) {
            Icon(
                painter = painterResource(id = R.drawable.ic_close),
                contentDescription = stringResource(id = R.string.microsurvey_close_button_content_description),
                tint = FirefoxTheme.colors.iconPrimary,
                modifier = Modifier.size(20.dp),
            )
        }
    }
}

@PreviewScreenSizes
@LightDarkPreview
@Preview(
    name = "Large Font",
    fontScale = 2.0f,
)
@Composable
private fun MicrosurveyHeaderPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
        ) {
            MicrosurveyHeader(stringResource(R.string.micro_survey_survey_header_2)) {}
        }
    }
}
