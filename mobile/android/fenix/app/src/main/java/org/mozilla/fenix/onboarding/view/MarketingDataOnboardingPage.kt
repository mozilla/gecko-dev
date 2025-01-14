/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
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
import androidx.compose.ui.res.painterResource
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
 * UI for an onboarding page that allows the user to opt out of marketing data analytics.
 *
 * @param state the UI state containing strings etc.
 * @param onMarketingDataContinueClick callback for when the user clicks the continue button.
 * @param onMarketingDataLearnMoreClick callback for when the user clicks the learn more text link.
 */
@Suppress("LongMethod")
@Composable
fun MarketingDataOnboardingPage(
    state: OnboardingPageState,
    onMarketingDataContinueClick: (allowMarketingDataCollection: Boolean) -> Unit,
    onMarketingDataLearnMoreClick: (url: String) -> Unit,
) {
    Column(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .padding(horizontal = 16.dp, vertical = 32.dp)
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.SpaceBetween,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        var checkboxChecked by remember { mutableStateOf(true) }

        Image(
            modifier = Modifier.weight(1f),
            painter = painterResource(id = state.imageRes),
            contentDescription = null,
        )

        Spacer(modifier = Modifier.height(32.dp))

        Text(
            text = state.title,
            color = FirefoxTheme.colors.textPrimary,
            textAlign = TextAlign.Center,
            style = FirefoxTheme.typography.headline5,
        )

        Spacer(modifier = Modifier.height(16.dp))

        state.marketingData?.let { marketingData ->
            LinkText(
                text = marketingData.bodyOneText,
                linkTextStates = listOf(
                    LinkTextState(
                        text = marketingData.bodyOneLinkText,
                        url = marketingData.linkUrl,
                        onClick = { url -> onMarketingDataLearnMoreClick(url) },
                    ),
                ),
                textAlign = TextAlign.Center,
            )

            Spacer(modifier = Modifier.height(22.dp))

            Row {
                Checkbox(
                    modifier = Modifier
                        .align(Alignment.Top)
                        .offset(
                            y = (-12).dp,
                            x = (-12).dp,
                        ),
                    checked = checkboxChecked,
                    onCheckedChange = { checkboxChecked = !checkboxChecked },
                    colors = CheckboxDefaults.colors(
                        checkedColor = FirefoxTheme.colors.formSelected,
                        uncheckedColor = FirefoxTheme.colors.formDefault,
                    ),
                )

                Column {
                    Text(
                        text = marketingData.bodyTwoText,
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.body2,
                        textAlign = TextAlign.Start,
                    )

                    Spacer(modifier = Modifier.height(8.dp))

                    Text(
                        text = marketingData.bodyThreeText,
                        color = FirefoxTheme.colors.textSecondary,
                        style = FirefoxTheme.typography.body2,
                        textAlign = TextAlign.Start,
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        PrimaryButton(
            modifier = Modifier
                .fillMaxWidth()
                .semantics {
                    testTag = state.title + "onboarding_card.positive_button"
                },
            text = state.primaryButton.text,
            onClick = { onMarketingDataContinueClick(checkboxChecked) },
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun MarketingDataOnboardingPagePreview() {
    FirefoxTheme {
        MarketingDataOnboardingPage(
            state = OnboardingPageState(
                imageRes = R.drawable.ic_high_five,
                title = "title",
                description = "description",
                primaryButton = Action(
                    text = "button",
                    onClick = {},
                ),
                marketingData = OnboardingMarketingData(
                    bodyOneText = "Unlike other companies, we take a balanced approach and never" +
                        " collect or store any sensitive information. Learn more",
                    bodyOneLinkText = "Learn more",
                    bodyTwoText = "Share minimal information with Mozilla and our marketing" +
                        " technology partners",
                    bodyThreeText = "This helps us understand how you discovered Firefox and" +
                        " improve our marketing campaigns.",
                    linkUrl = "",
                ),
            ),
            onMarketingDataContinueClick = {},
            onMarketingDataLearnMoreClick = {},
        )
    }
}
