/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.toggleable
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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
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
 * @param onMarketingDataLearnMoreClick callback for when the user clicks the learn more text link.
 * @param onMarketingOptInToggle callback for when the user toggles the opt-in checkbox.
 * @param onMarketingDataContinueClick callback for when the user clicks the continue button.
 */
@Suppress("LongMethod")
@Composable
fun MarketingDataOnboardingPage(
    state: OnboardingPageState,
    onMarketingDataLearnMoreClick: () -> Unit,
    onMarketingOptInToggle: (optIn: Boolean) -> Unit,
    onMarketingDataContinueClick: (allowMarketingDataCollection: Boolean) -> Unit,
) {
    BoxWithConstraints(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .padding(bottom = 24.dp),
    ) {
        val boxWithConstraintsScope = this

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState()),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.SpaceBetween,
        ) {
            var checkboxChecked by remember { mutableStateOf(true) }

            Spacer(Modifier)

            Column(
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 32.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Image(
                    painter = painterResource(id = state.imageRes),
                    contentDescription = null,
                    modifier = Modifier.height(imageHeight(boxWithConstraintsScope)),
                )

                Spacer(modifier = Modifier.height(32.dp))

                Text(
                    text = state.title,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.headline5,
                )

                Spacer(modifier = Modifier.height(16.dp))

                Text(
                    modifier = Modifier.padding(horizontal = 20.dp),
                    text = state.description,
                    color = FirefoxTheme.colors.textSecondary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.body2,
                )
            }

            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.padding(horizontal = 16.dp),
            ) {
                state.marketingData?.let { marketingData ->
                    Row(
                        Modifier.toggleable(
                            value = checkboxChecked,
                            role = Role.Checkbox,
                            onValueChange = { checkboxChecked = !checkboxChecked },
                        ),
                    ) {
                        Checkbox(
                            modifier = Modifier
                                .align(Alignment.Top)
                                .offset(y = (-12).dp, x = (-12).dp)
                                .clearAndSetSemantics {},
                            checked = checkboxChecked,
                            onCheckedChange = {
                                checkboxChecked = !checkboxChecked
                                onMarketingOptInToggle.invoke(checkboxChecked)
                            },
                            colors = CheckboxDefaults.colors(
                                checkedColor = FirefoxTheme.colors.formSelected,
                                uncheckedColor = FirefoxTheme.colors.formDefault,
                            ),
                        )

                        Text(
                            text = marketingData.bodyTwoText,
                            color = FirefoxTheme.colors.textPrimary,
                            style = FirefoxTheme.typography.body2,
                            textAlign = TextAlign.Start,
                        )
                    }
                    Spacer(modifier = Modifier.height(16.dp))
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally,
                        modifier = Modifier.padding(horizontal = 16.dp),
                    ) {
                        LinkText(
                            text = marketingData.bodyOneText,
                            linkTextStates = listOf(
                                LinkTextState(
                                    text = marketingData.bodyOneLinkText,
                                    url = "",
                                    onClick = { onMarketingDataLearnMoreClick() },
                                ),
                            ),
                            textAlign = TextAlign.Center,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))

                PrimaryButton(
                    modifier = Modifier
                        .width(width = FirefoxTheme.layout.size.maxWidth.small)
                        .semantics { testTag = state.title + "onboarding_card.positive_button" },
                    text = state.primaryButton.text,
                    onClick = { onMarketingDataContinueClick(checkboxChecked) },
                )
            }
        }
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun MarketingDataOnboardingPagePreview() {
    FirefoxTheme {
        MarketingDataOnboardingPage(
            state = OnboardingPageState(
                imageRes = R.drawable.ic_onboarding_welcome,
                title = stringResource(id = R.string.onboarding_marketing_title),
                description = stringResource(id = R.string.onboarding_marketing_body),
                primaryButton = Action(
                    text = stringResource(id = R.string.onboarding_marketing_positive_button),
                    onClick = {},
                ),
                marketingData = OnboardingMarketingData(
                    bodyOneText = stringResource(id = R.string.onboarding_marketing_learn_more),
                    bodyOneLinkText = stringResource(id = R.string.onboarding_marketing_learn_more),
                    bodyTwoText = stringResource(id = R.string.onboarding_marketing_opt_in_checkbox),
                ),
            ),
            onMarketingDataLearnMoreClick = {},
            onMarketingOptInToggle = {},
            onMarketingDataContinueClick = {},
        )
    }
}
