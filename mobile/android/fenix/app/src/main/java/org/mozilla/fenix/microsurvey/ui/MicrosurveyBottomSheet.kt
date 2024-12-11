/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.annotation.DrawableRes
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Scaffold
import androidx.compose.material.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.traversalIndex
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewScreenSizes
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.theme.FirefoxTheme

private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f
private val bottomSheetShape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp)

/**
 * The microsurvey bottom sheet.
 *
 * @param question The question text.
 * @param answers The answer text options available for the given [question].
 * @param icon The icon that represents the feature for the given [question].
 * @param onPrivacyPolicyLinkClick Invoked when the privacy policy link is clicked.
 * @param onCloseButtonClicked Invoked when the close button is clicked.
 * @param onSubmitButtonClicked Invoked when the submit button is clicked.
 */
@Composable
fun MicrosurveyBottomSheet(
    question: String,
    answers: List<String>,
    @DrawableRes icon: Int,
    onPrivacyPolicyLinkClick: () -> Unit,
    onCloseButtonClicked: () -> Unit,
    onSubmitButtonClicked: (String) -> Unit,
) {
    var selectedAnswer by remember { mutableStateOf<String?>(null) }
    var isSubmitted by remember { mutableStateOf(false) }

    Scaffold(
        backgroundColor = FirefoxTheme.colors.layer1,
        topBar = {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier
                    .padding(top = 8.dp)
                    .nestedScroll(rememberNestedScrollInteropConnection())
                    .verticalScroll(rememberScrollState()),
            ) {
                BottomSheetHandle(
                    onRequestDismiss = {},
                    contentDescription = stringResource(R.string.microsurvey_close_handle_content_description),
                    modifier = Modifier
                        .padding(bottom = 2.dp)
                        .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                        .semantics { traversalIndex = -1f },
                )

                MicrosurveyHeader(title = stringResource(id = R.string.micro_survey_survey_header_2)) {
                    onCloseButtonClicked()
                }
            }
        },
        bottomBar = {
            Column(
                modifier = Modifier
                    .padding(bottom = 8.dp),
            ) {
                Spacer(modifier = Modifier.height(24.dp))

                MicrosurveyFooter(
                    isSubmitted = isSubmitted,
                    isContentAnswerSelected = selectedAnswer != null,
                    onPrivacyPolicyLinkClick = onPrivacyPolicyLinkClick,
                    onButtonClick = {
                        selectedAnswer?.let {
                            onSubmitButtonClicked(it)
                            isSubmitted = true
                        }
                    },
                )
            }
        },
    ) { innerPadding ->
        Surface(
            modifier = Modifier
                .wrapContentHeight()
                .padding(innerPadding),
            color = FirefoxTheme.colors.layer1,
            shape = bottomSheetShape,
        ) {
            if (isSubmitted) {
                MicrosurveyCompleted()
            } else {
                MicrosurveyContent(
                    question = question,
                    icon = icon,
                    answers = answers,
                    selectedAnswer = selectedAnswer,
                    onSelectionChange = { selectedAnswer = it },
                )
            }
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
private fun MicrosurveyBottomSheetPreview() {
    FirefoxTheme {
        MicrosurveyBottomSheet(
            question = "How satisfied are you with printing in Firefox?",
            icon = R.drawable.ic_print,
            onPrivacyPolicyLinkClick = {},
            onCloseButtonClicked = {},
            onSubmitButtonClicked = {},
            answers = listOf(
                stringResource(id = R.string.likert_scale_option_1),
                stringResource(id = R.string.likert_scale_option_2),
                stringResource(id = R.string.likert_scale_option_3),
                stringResource(id = R.string.likert_scale_option_4),
                stringResource(id = R.string.likert_scale_option_5),
                stringResource(id = R.string.likert_scale_option_6),
            ),
        )
    }
}
