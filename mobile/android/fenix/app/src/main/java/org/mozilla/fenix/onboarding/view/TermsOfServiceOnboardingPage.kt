/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A Composable for displaying the terms of service onboarding page content.
 *
 * @param pageState The page content that's displayed.
 * @param eventHandler The event handler for all user interactions of this page.
 */
@Composable
fun TermsOfServiceOnboardingPage(
    pageState: OnboardingPageState,
    eventHandler: OnboardingTermsOfServiceEventHandler,
) {
    // Base
    Column(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .padding(horizontal = 16.dp, vertical = 32.dp)
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.SpaceBetween,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        with(pageState) {
            // Main content group
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                HeaderImage(imageRes)

                Spacer(Modifier.height(25.dp))

                Text(
                    text = title,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.headline5,
                )
            }

            Spacer(Modifier.height(50.dp))

            BodyText(pageState, eventHandler)

            PrimaryButton(
                text = primaryButton.text,
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { testTag = title + "onboarding_card.positive_button" },
                onClick = primaryButton.onClick,
            )
        }
    }

    LaunchedEffect(pageState) {
        pageState.onRecordImpressionEvent()
    }
}

@Composable
private fun BodyText(
    pageState: OnboardingPageState,
    eventHandler: OnboardingTermsOfServiceEventHandler,
) {
    pageState.termsOfService?.let {
        Column(
            modifier = Modifier.padding(horizontal = 24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            val lineOneState = LinkTextState(
                text = it.lineOneLinkText,
                url = it.lineOneLinkUrl,
                onClick = eventHandler::onTermsOfServiceLinkClicked,
            )
            val lineTwoState = LinkTextState(
                text = it.lineTwoLinkText,
                url = it.lineTwoLinkUrl,
                onClick = eventHandler::onPrivacyNoticeLinkClicked,
            )
            val lineThreeState = LinkTextState(
                text = it.lineThreeLinkText,
                url = "",
                onClick = { _ -> eventHandler.onManagePrivacyPreferencesLinkClicked() },
            )
            LinkText(
                text = it.lineOneText.updateFirstPlaceholder(it.lineOneLinkText),
                linkTextStates = listOf(
                    lineOneState,
                ),
                style = FirefoxTheme.typography.caption.copy(
                    textAlign = TextAlign.Center,
                    color = FirefoxTheme.colors.textSecondary,
                ),
            )
            Spacer(Modifier.height(8.dp))
            LinkText(
                text = it.lineTwoText.updateFirstPlaceholder(it.lineTwoLinkText),
                linkTextStates = listOf(
                    lineTwoState,
                ),
                style = FirefoxTheme.typography.caption.copy(
                    textAlign = TextAlign.Center,
                    color = FirefoxTheme.colors.textSecondary,
                ),
            )
            Spacer(Modifier.height(8.dp))
            LinkText(
                text = it.lineThreeText.updateFirstPlaceholder(it.lineThreeLinkText),
                linkTextStates = listOf(
                    lineThreeState,
                ),
                style = FirefoxTheme.typography.caption.copy(
                    textAlign = TextAlign.Center,
                    color = FirefoxTheme.colors.textSecondary,
                ),
            )
        }
    }
}

@Composable
private fun HeaderImage(@DrawableRes imageRes: Int) {
    Spacer(Modifier.height(75.dp))

    Image(
        painter = painterResource(id = imageRes),
        contentDescription = null,
        modifier = Modifier
            .height(167.dp)
            .width(161.dp),
    )
}

private fun String.updateFirstPlaceholder(text: String) = replace("%1\$s", text)

// *** Code below used for previews only *** //

@FlexibleWindowLightDarkPreview
@Composable
private fun OnboardingPagePreview() {
    FirefoxTheme {
        TermsOfServiceOnboardingPage(
            pageState = OnboardingPageState(
                title = stringResource(id = R.string.onboarding_welcome_to_firefox),
                description = "",
                termsOfService = OnboardingTermsOfService(
                    lineOneText = stringResource(id = R.string.onboarding_term_of_service_line_one),
                    lineOneLinkText = stringResource(id = R.string.onboarding_term_of_service_line_one_link_text),
                    lineOneLinkUrl = "URL",
                    lineTwoText = stringResource(id = R.string.onboarding_term_of_service_line_two),
                    lineTwoLinkText = stringResource(id = R.string.onboarding_term_of_service_line_two_link_text),
                    lineTwoLinkUrl = "URL",
                    lineThreeText = stringResource(id = R.string.onboarding_term_of_service_line_three),
                    lineThreeLinkText = stringResource(id = R.string.onboarding_term_of_service_line_three_link_text),
                ),
                imageRes = R.drawable.ic_firefox,
                primaryButton = Action(
                    text = stringResource(
                        id = R.string.onboarding_term_of_service_agree_and_continue_button_label,
                    ),
                    onClick = {},
                ),
            ),
            eventHandler = object : OnboardingTermsOfServiceEventHandler {},
        )
    }
}
