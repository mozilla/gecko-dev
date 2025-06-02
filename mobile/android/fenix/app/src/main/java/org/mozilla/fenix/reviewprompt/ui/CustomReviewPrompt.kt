/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt.ui

import androidx.compose.animation.Crossfade
import androidx.compose.animation.animateContentSize
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.PrimaryButton
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.reviewprompt.CustomReviewPromptState
import org.mozilla.fenix.reviewprompt.CustomReviewPromptState.Feedback
import org.mozilla.fenix.reviewprompt.CustomReviewPromptState.PrePrompt
import org.mozilla.fenix.reviewprompt.CustomReviewPromptState.Rate
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Prompt that can show either:
 * - initial state asking to rate the experience,
 * - state asking to leave a Play Store rating,
 * - or state asking to leave feedback.
 *
 * @param state The state (or step) the prompt should be showing.
 * @param onRequestDismiss Called when the accessibility affordance to dismiss the prompt is clicked.
 * @param onIssuesButtonClick Called when the negative button in the pre-prompt is clicked.
 * @param onGreatButtonClick Called when the positive button in the pre-prompt is clicked.
 * @param onRateButtonClick Called when the rate on Play Store button is clicked.
 * @param onLeaveFeedbackButtonClick Called when the leave feedback button is clicked.
 * @param modifier The modifier to be applied to the prompt.
 */
@Composable
fun CustomReviewPrompt(
    state: CustomReviewPromptState,
    onRequestDismiss: () -> Unit,
    onIssuesButtonClick: () -> Unit,
    onGreatButtonClick: () -> Unit,
    onRateButtonClick: () -> Unit,
    onLeaveFeedbackButtonClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    BottomSheet(
        onRequestDismiss,
        modifier.animateContentSize(),
    ) {
        Crossfade(state) { state ->
            when (state) {
                PrePrompt -> PrePrompt(
                    onIssuesButtonClick = onIssuesButtonClick,
                    onGreatButtonClick = onGreatButtonClick,
                )

                Rate -> RateStep(onRateButtonClick = onRateButtonClick)
                Feedback -> FeedbackStep(onLeaveFeedbackButtonClick = onLeaveFeedbackButtonClick)
            }
        }
    }
}

@Composable
private fun BottomSheet(
    onRequestDismiss: () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    Box(
        modifier.fillMaxWidth(),
        Alignment.BottomCenter,
    ) {
        Column(
            Modifier
                .clip(RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp))
                .background(FirefoxTheme.colors.layer3)
                .widthIn(max = FirefoxTheme.layout.size.maxWidth.medium)
                .fillMaxWidth()
                .padding(horizontal = 20.dp)
                .padding(bottom = 16.dp),
        ) {
            BottomSheetHandle(
                onRequestDismiss = onRequestDismiss,
                contentDescription = stringResource(R.string.mozac_cfr_dismiss_button_content_description),
                modifier = Modifier
                    .align(Alignment.CenterHorizontally)
                    .padding(all = 16.dp)
                    .width(32.dp),
            )

            content()
        }
    }
}

@Composable
private fun PrePrompt(
    onIssuesButtonClick: () -> Unit,
    onGreatButtonClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier) {
        Text(
            stringResource(
                R.string.review_prompt_pre_prompt_header,
                stringResource(R.string.firefox),
            ),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline7,
        )

        Spacer(Modifier.height(20.dp))

        Row {
            FoxEmojiButton(
                emoji = painterResource(R.drawable.review_prompt_negative_button),
                label = stringResource(R.string.review_prompt_negative_button),
                onClick = onIssuesButtonClick,
                modifier = Modifier.weight(1f),
            )

            Spacer(Modifier.width(20.dp))

            FoxEmojiButton(
                emoji = painterResource(R.drawable.review_prompt_positive_button),
                label = stringResource(R.string.review_prompt_positive_button),
                onClick = onGreatButtonClick,
                modifier = Modifier.weight(1f),
            )
        }
    }
}

@Composable
private fun FoxEmojiButton(
    emoji: Painter,
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier
            .height(100.dp)
            .clip(RoundedCornerShape(size = 18.dp))
            .border(1.dp, FirefoxTheme.colors.borderPrimary, RoundedCornerShape(size = 18.dp))
            .background(FirefoxTheme.colors.layer1)
            .clickable(onClick = onClick),
        Arrangement.Center,
        Alignment.CenterHorizontally,
    ) {
        Image(emoji, contentDescription = null)

        Spacer(Modifier.height(10.dp))

        Text(
            label,
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
        )
    }
}

@Composable
private fun RateStep(onRateButtonClick: () -> Unit, modifier: Modifier = Modifier) {
    Column(modifier.padding(vertical = 16.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Image(
                painterResource(R.drawable.review_prompt_positive_button),
                contentDescription = null,
            )

            Spacer(Modifier.width(10.dp))

            Text(
                stringResource(
                    R.string.review_prompt_rate_header,
                    stringResource(R.string.firefox),
                ),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline7,
            )
        }

        Spacer(Modifier.height(20.dp))

        PrimaryButton(
            stringResource(R.string.review_prompt_rate_button, stringResource(R.string.firefox)),
            Modifier.fillMaxWidth(),
            onClick = onRateButtonClick,
        )
    }
}

@Composable
private fun FeedbackStep(onLeaveFeedbackButtonClick: () -> Unit, modifier: Modifier = Modifier) {
    Column(modifier.padding(vertical = 16.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Image(
                painterResource(R.drawable.review_prompt_negative_button),
                contentDescription = null,
            )

            Spacer(Modifier.width(10.dp))

            Text(
                stringResource(
                    R.string.review_prompt_feedback_header,
                    stringResource(R.string.firefox),
                ),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline7,
            )
        }

        Spacer(Modifier.height(20.dp))

        PrimaryButton(
            stringResource(R.string.review_prompt_feedback_button),
            Modifier.fillMaxWidth(),
            onClick = onLeaveFeedbackButtonClick,
        )
    }
}

@PreviewLightDark
@Composable
private fun PrePromptPreview() {
    FirefoxTheme {
        CustomReviewPrompt(
            state = PrePrompt,
            onRequestDismiss = {},
            onIssuesButtonClick = {},
            onGreatButtonClick = {},
            onRateButtonClick = {},
            onLeaveFeedbackButtonClick = {},
        )
    }
}

@PreviewLightDark
@Composable
private fun RatePromptPreview() {
    FirefoxTheme {
        CustomReviewPrompt(
            state = Rate,
            onRequestDismiss = {},
            onIssuesButtonClick = {},
            onGreatButtonClick = {},
            onRateButtonClick = {},
            onLeaveFeedbackButtonClick = {},
        )
    }
}

@PreviewLightDark
@Composable
private fun FeedbackPromptPreview() {
    FirefoxTheme {
        CustomReviewPrompt(
            state = Feedback,
            onRequestDismiss = {},
            onIssuesButtonClick = {},
            onGreatButtonClick = {},
            onRateButtonClick = {},
            onLeaveFeedbackButtonClick = {},
        )
    }
}

@PreviewLightDark
@Composable
private fun FoxEmojiButtonPreview() {
    FirefoxTheme {
        FoxEmojiButton(
            emoji = painterResource(R.drawable.review_prompt_positive_button),
            label = "It’s great!",
            onClick = {},
            modifier = Modifier
                .padding(16.dp)
                .width(176.dp),
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun BottomSheetPreview() {
    FirefoxTheme {
        BottomSheet(
            onRequestDismiss = {},
        ) {
            Box(
                Modifier
                    .size(64.dp)
                    .clip(CircleShape)
                    .background(Color.Red),
            )
        }
    }
}

@Preview
@Composable
private fun InteractiveCustomReviewPromptPreview() {
    var promptState by remember { mutableStateOf(PrePrompt) }
    FirefoxTheme {
        Box(
            Modifier.height(224.dp),
            Alignment.BottomCenter,
        ) {
            CustomReviewPrompt(
                state = promptState,
                onRequestDismiss = { },
                onIssuesButtonClick = { promptState = Feedback },
                onGreatButtonClick = { promptState = Rate },
                onRateButtonClick = { promptState = PrePrompt },
                onLeaveFeedbackButtonClick = { promptState = PrePrompt },
            )
        }
    }
}
