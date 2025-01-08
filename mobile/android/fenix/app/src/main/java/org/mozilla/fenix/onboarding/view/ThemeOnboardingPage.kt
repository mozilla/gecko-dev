/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import android.content.Context
import androidx.appcompat.app.AppCompatDelegate
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.utils.inComposePreview
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.support.ktx.android.content.setApplicationNightMode
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.onboarding.store.OnboardingStore
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A Composable for displaying theme selection onboarding page content.
 *
 * @param onboardingStore The [OnboardingStore] that holds the theme selection state.
 * @param pageState The page content that's displayed.
 * @param onThemeSelectionClicked Callback for when a theme selection is clicked.
 */
@Suppress("LongMethod")
@Composable
fun ThemeOnboardingPage(
    onboardingStore: OnboardingStore,
    pageState: OnboardingPageState,
    onThemeSelectionClicked: (ThemeOptionType) -> Unit,
) {
    // Base
    Column(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .padding(horizontal = 16.dp, vertical = 24.dp)
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.SpaceBetween,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        with(pageState) {
            // Main content group
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Spacer(Modifier.height(18.dp))
                Image(
                    painter = painterResource(id = imageRes),
                    contentDescription = stringResource(
                        R.string.onboarding_customize_theme_main_image_content_description,
                    ),
                    modifier = Modifier.width(263.dp),
                )

                Spacer(Modifier.height(52.dp))

                Text(
                    text = title,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.headline5,
                )

                Spacer(Modifier.height(8.dp))

                Text(
                    text = description,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.body2,
                )

                Spacer(Modifier.height(32.dp))

                val state by onboardingStore.observeAsState(initialValue = onboardingStore.state) { state -> state }

                if (!inComposePreview) {
                    LocalContext.current.applyThemeIfRequired(state.themeOptionSelected)
                }

                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.Center) {
                    themeOptions?.let {
                        ThemeOptions(
                            options = it,
                            selectedOption = state.themeOptionSelected,
                            onClick = onThemeSelectionClicked,
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            Column {
                PrimaryButton(
                    text = primaryButton.text,
                    modifier = Modifier
                        .fillMaxWidth()
                        .semantics { testTag = title + "onboarding_card.positive_button" },
                    onClick = { primaryButton.onClick() },
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

/**
 * Applies the selected theme to the application if different to the current theme.
 *
 * This function uses [AppCompatDelegate] to change the application's theme
 * based on the user's selection. It supports the following themes:
 *
 * - Dark Theme: Forces the application into dark mode.
 * - Light Theme: Forces the application into light mode.
 * - System Theme: Adapts to the device's current system theme.
 *
 * @param selectedTheme The [ThemeOptionType] selected by the user.
 * This determines which theme to apply.
 */
fun Context.applyThemeIfRequired(selectedTheme: ThemeOptionType) {
    setApplicationNightMode(
        when (selectedTheme) {
            ThemeOptionType.THEME_DARK -> AppCompatDelegate.MODE_NIGHT_YES
            ThemeOptionType.THEME_LIGHT -> AppCompatDelegate.MODE_NIGHT_NO
            ThemeOptionType.THEME_SYSTEM -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
        },
    )
}

@Composable
private fun ThemeOptions(
    options: List<ThemeOption>,
    selectedOption: ThemeOptionType,
    onClick: (ThemeOptionType) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center,
    ) {
        options.forEach {
            SelectableImageItem(
                themeOption = it,
                selectedOption = selectedOption,
                onClick = onClick,
            )

            if (it != options.last()) {
                Spacer(Modifier.width(26.dp))
            }
        }
    }
}

@Composable
private fun SelectableImageItem(
    themeOption: ThemeOption,
    selectedOption: ThemeOptionType,
    onClick: (ThemeOptionType) -> Unit,
) {
    val isSelectedOption = themeOption.themeType == selectedOption

    Column(
        modifier = Modifier.clickable(onClick = {
            if (!isSelectedOption) {
                onClick(themeOption.themeType)
            }
        }),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Image(
            painter = painterResource(id = themeOption.imageRes),
            contentDescription = stringResource(
                R.string.onboarding_customize_theme_content_description,
                themeOption.label,
            ),
            modifier = if (isSelectedOption) {
                Modifier.border(2.dp, FirefoxTheme.colors.actionPrimary, RoundedCornerShape(10.dp))
            } else {
                Modifier
            },
        )

        Text(
            text = themeOption.label,
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
        } else {
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .border(2.dp, FirefoxTheme.colors.actionSecondary, CircleShape),
                contentAlignment = Alignment.Center,
            ) {
            }
        }
    }
}

// *** Code below used for previews only *** //

@FlexibleWindowLightDarkPreview
@Composable
private fun OnboardingPagePreview() {
    FirefoxTheme {
        ThemeOnboardingPage(
            onboardingStore = OnboardingStore(),
            pageState = OnboardingPageState(
                imageRes = R.drawable.ic_pick_a_theme,
                title = stringResource(id = R.string.onboarding_customize_theme_title),
                description = stringResource(id = R.string.onboarding_customize_theme_description),
                primaryButton = Action(
                    text = stringResource(
                        id = R.string.onboarding_save_and_continue_button,
                    ),
                    onClick = {},
                ),
                secondaryButton = Action(
                    text = stringResource(
                        id = R.string.onboarding_customize_theme_not_now_button,
                    ),
                    onClick = {},
                ),
                themeOptions = listOf(
                    ThemeOption(
                        themeType = ThemeOptionType.THEME_DARK,
                        imageRes = R.drawable.ic_pick_a_theme_dark,
                        label = stringResource(R.string.onboarding_customize_theme_dark_option),
                    ),
                    ThemeOption(
                        themeType = ThemeOptionType.THEME_LIGHT,
                        imageRes = R.drawable.ic_pick_a_theme_light,
                        label = stringResource(R.string.onboarding_customize_theme_light_option),
                    ),
                    ThemeOption(
                        themeType = ThemeOptionType.THEME_SYSTEM,
                        imageRes = R.drawable.ic_pick_a_theme_system_auto,
                        label = stringResource(R.string.onboarding_customize_theme_system_option),
                    ),
                ),
                onRecordImpressionEvent = {},
            ),
            onThemeSelectionClicked = {},
        )
    }
}
