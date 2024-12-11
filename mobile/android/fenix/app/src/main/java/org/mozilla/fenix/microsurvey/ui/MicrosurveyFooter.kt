/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.PreviewScreenSizes
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The footer UI used for microsurvey.
 *
 * @param isSubmitted Whether the user has "Submitted" the survey or not.
 * @param isContentAnswerSelected Whether the user clicked on one of the answers or not.
 * @param onPrivacyPolicyLinkClick Invoked when the privacy policy link is clicked.
 * @param onButtonClick Invoked when the "Submit"/"Close" button is clicked.
 */
@Composable
fun MicrosurveyFooter(
    isSubmitted: Boolean,
    isContentAnswerSelected: Boolean,
    onPrivacyPolicyLinkClick: () -> Unit,
    onButtonClick: () -> Unit,
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp),
    ) {
        if (!isSubmitted) {
            PrimaryButton(
                text = stringResource(id = R.string.micro_survey_submit_button_label),
                enabled = isContentAnswerSelected,
                onClick = { onButtonClick() },
            )
        }

        Spacer(modifier = Modifier.height(12.dp))

        LinkText(
            text = stringResource(id = R.string.micro_survey_privacy_notice_2),
            linkTextStates = listOf(
                LinkTextState(
                    text = stringResource(id = R.string.micro_survey_privacy_notice_2),
                    url = "",
                    onClick = { onPrivacyPolicyLinkClick() },
                ),
            ),
            style = FirefoxTheme.typography.caption,
            linkTextDecoration = TextDecoration.Underline,
        )
    }
}

@PreviewScreenSizes
@LightDarkPreview
@Composable
private fun ReviewQualityCheckFooterPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            MicrosurveyFooter(
                isSubmitted = false,
                isContentAnswerSelected = false,
                onPrivacyPolicyLinkClick = {},
                onButtonClick = {},
            )

            MicrosurveyFooter(
                isSubmitted = false,
                isContentAnswerSelected = true,
                onPrivacyPolicyLinkClick = {},
                onButtonClick = {},
            )

            MicrosurveyFooter(
                isSubmitted = true,
                isContentAnswerSelected = true,
                onPrivacyPolicyLinkClick = {},
                onButtonClick = {},
            )
        }
    }
}
