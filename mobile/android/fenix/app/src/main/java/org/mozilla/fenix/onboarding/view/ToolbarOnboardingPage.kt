/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.onboarding.store.OnboardingStore
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A Composable for displaying toolbar placement onboarding page content.
 *
 * @param onboardingStore The [OnboardingStore] that holds the toolbar selection state.
 * @param pageState The page content that's displayed.
 * @param onToolbarSelectionClicked Callback for when a toolbar selection is clicked.
 */
@Composable
fun ToolbarOnboardingPage(
    onboardingStore: OnboardingStore,
    pageState: OnboardingPageState,
    onToolbarSelectionClicked: (ToolbarOptionType) -> Unit,
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
                Spacer(Modifier.height(50.dp))

                Image(
                    painter = painterResource(id = imageRes),
                    contentDescription = stringResource(
                        R.string.onboarding_customize_toolbar_main_image_content_description,
                    ),
                    modifier = Modifier.width(323.dp),
                )

                Spacer(Modifier.height(84.dp))

                Text(
                    text = title,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.headline5,
                )

                Spacer(Modifier.height(16.dp))

                Text(
                    text = description,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.body2,
                )

                Spacer(Modifier.height(34.dp))

                val state by onboardingStore.observeAsState(initialValue = onboardingStore.state) { state -> state }

                Row(Modifier.width(176.dp), horizontalArrangement = Arrangement.Center) {
                    ToolbarOptions(
                        options = toolbarOptions!!,
                        selectedOption = state.toolbarOptionSelected,
                        onClick = onToolbarSelectionClicked,
                    )
                }
            }

            Column {
                PrimaryButton(
                    text = primaryButton.text,
                    modifier = Modifier
                        .fillMaxWidth()
                        .semantics { testTag = title + "onboarding_card.positive_button" },
                    onClick = primaryButton.onClick,
                )

                Spacer(modifier = Modifier.height(8.dp))

                pageState.secondaryButton?.let {
                    SecondaryButton(
                        modifier = Modifier
                            .fillMaxWidth()
                            .semantics {
                                testTag = pageState.title + "onboarding_card.negative_button"
                            },
                        text = it.text,
                        onClick = it.onClick,
                    )
                }
            }
        }
    }

    LaunchedEffect(pageState) {
        pageState.onRecordImpressionEvent()
    }
}

@Composable
private fun ToolbarOptions(
    options: List<ToolbarOption>,
    selectedOption: ToolbarOptionType,
    onClick: (ToolbarOptionType) -> Unit,
) {
    SelectableImageItem(
        toolbarOption = options[0],
        selectedOption = selectedOption,
        onClick = onClick,
    )

    Spacer(Modifier.width(40.dp))

    SelectableImageItem(
        toolbarOption = options[1],
        selectedOption = selectedOption,
        onClick = onClick,
    )
}

@Composable
private fun SelectableImageItem(
    toolbarOption: ToolbarOption,
    selectedOption: ToolbarOptionType,
    onClick: (ToolbarOptionType) -> Unit,
) {
    val isSelectedOption = toolbarOption.toolbarType == selectedOption

    Column(
        modifier = Modifier.clickable(
            onClick = {
                // Only call action if option selected is different.
                if (!isSelectedOption) {
                    onClick(toolbarOption.toolbarType)
                }
            },
        ),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Image(
            painter = painterResource(id = toolbarOption.imageRes),
            contentDescription = stringResource(
                R.string.onboarding_customize_toolbar_placement_content_description,
                toolbarOption.label,
            ),
            modifier = if (isSelectedOption) {
                Modifier.border(2.dp, FirefoxTheme.colors.actionPrimary, RoundedCornerShape(10.dp))
            } else {
                Modifier
            },
        )

        Text(
            text = toolbarOption.label,
            color = FirefoxTheme.colors.textPrimary,
            modifier = Modifier.padding(vertical = 6.dp),
            style = FirefoxTheme.typography.caption,
        )

        if (isSelectedOption) {
            Box(
                modifier = Modifier
                    .background(
                        color = FirefoxTheme.colors.layerAccent,
                        shape = CircleShape,
                    )
                    .size(24.dp),
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_checkmark_24),
                    contentDescription = null, // decorative only.
                    modifier = Modifier.size(12.dp),
                    tint = PhotonColors.White,
                )
            }
        }
    }
}

// *** Code below used for previews only *** //

@FlexibleWindowLightDarkPreview
@Composable
private fun OnboardingPagePreview() {
    FirefoxTheme {
        ToolbarOnboardingPage(
            onboardingStore = OnboardingStore(),
            pageState = OnboardingPageState(
                imageRes = R.drawable.ic_onboarding_customize_toolbar,
                title = stringResource(id = R.string.onboarding_customize_toolbar_title),
                description = stringResource(id = R.string.onboarding_customize_toolbar_description),
                primaryButton = Action(
                    text = stringResource(
                        id = R.string.onboarding_customize_toolbar_save_and_continue_button,
                    ),
                    onClick = {},
                ),
                secondaryButton = Action(
                    text = stringResource(
                        id = R.string.onboarding_customize_toolbar_skip_button,
                    ),
                    onClick = {},
                ),
                toolbarOptions = listOf(
                    ToolbarOption(
                        toolbarType = ToolbarOptionType.TOOLBAR_TOP,
                        imageRes = R.drawable.ic_onboarding_top_toolbar,
                        label = stringResource(R.string.onboarding_customize_toolbar_top_option),
                    ),
                    ToolbarOption(
                        toolbarType = ToolbarOptionType.TOOLBAR_BOTTOM,
                        imageRes = R.drawable.ic_onboarding_bottom_toolbar,
                        label = stringResource(R.string.onboarding_customize_toolbar_bottom_option),
                    ),
                ),
                onRecordImpressionEvent = {},
            ),
            onToolbarSelectionClicked = {},
        )
    }
}
