/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh.info

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Composable function that displays the info screen of DoH settings.
 *
 * @param infoScreenTopic The [infoScreenTopic] that we would like to display.
 * @param onNavigateUp Invoked when the user clicks the navigate up (back) button.
 * @param onLearnMoreClicked Invoked when the user wants to visit an external doc about DoH.
 */
@Composable
@Suppress("SpreadOperator")
internal fun InfoScreen(
    infoScreenTopic: InfoScreenTopic,
    onNavigateUp: () -> Unit = {},
    onLearnMoreClicked: (String) -> Unit = {},
) {
    val title = stringResource(infoScreenTopic.titleId)
    val bulletPoints = infoScreenTopic.bulletPoints.map { (bulletPoint, sumoTopic) ->
        val placeholders = bulletPoint.placeholders.map { placeholderRes ->
            stringResource(placeholderRes)
        }

        // If placeholders exist, pass them in the correct order to stringResource
        val bulletText = if (placeholders.isNotEmpty()) {
            stringResource(bulletPoint.textRes, *placeholders.toTypedArray())
        } else {
            stringResource(bulletPoint.textRes)
        }

        // Return the final text and the url
        bulletText to sumoTopic
    }

    Scaffold(
        topBar = {
            Toolbar(
                title = title,
                onToolbarBackClick = onNavigateUp,
            )
        },
        backgroundColor = FirefoxTheme.colors.layer1,
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues),
        ) {
            Title(
                title = title,
            )

            bulletPoints.forEach { (text, url) ->
                val learnMoreUrl = url?.let {
                    SupportUtils.getGenericSumoURLForTopic(it)
                }
                BulletTextWithOptionalLink(
                    text = text,
                    learnMoreUrl = learnMoreUrl,
                    onLearnMoreClicked = onLearnMoreClicked,
                )
            }
        }
    }
}

@Composable
private fun Toolbar(
    title: String,
    onToolbarBackClick: () -> Unit,
) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
                text = title,
            )
        },
        navigationIcon = {
            IconButton(onClick = onToolbarBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.preference_doh_up_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
    )
}

@Composable
private fun Title(
    title: String,
) {
    Row(
        modifier = Modifier
            .padding(
                start = 72.dp,
                top = 6.dp,
                end = 16.dp,
                bottom = 6.dp,
            ),
    ) {
        Text(
            text = title,
            color = FirefoxTheme.colors.textAccent,
            style = FirefoxTheme.typography.headline8,
        )
    }
}

@Composable
private fun BulletTextWithOptionalLink(
    text: String,
    learnMoreUrl: String? = null,
    onLearnMoreClicked: (String) -> Unit,
    modifier: Modifier = Modifier
        .padding(
            start = 72.dp,
            top = 6.dp,
            end = 16.dp,
            bottom = 6.dp,
        ),
    color: Color = FirefoxTheme.colors.textPrimary,
    style: TextStyle = FirefoxTheme.typography.subtitle1,
) {
    Row(
        modifier = modifier,
    ) {
        Text(
            text = "•",
            modifier = Modifier.padding(end = 8.dp),
            color = color,
        )

        if (learnMoreUrl == null) {
            Text(
                text = text,
                color = color,
                style = style,
            )
        } else {
            LinkText(
                text = text,
                linkTextStates = listOf(
                    LinkTextState(
                        text = stringResource(R.string.preference_doh_learn_more),
                        url = learnMoreUrl,
                        onClick = { onLearnMoreClicked(it) },
                    ),
                ),
                linkTextDecoration = TextDecoration.Underline,
                style = style.copy(
                    color = color,
                ),
            )
        }
    }
}

@Composable
@FlexibleWindowLightDarkPreview
private fun InfoScreenPreview() {
    FirefoxTheme {
        InfoScreen(
            infoScreenTopic = InfoScreenTopic.DEFAULT,
        )
    }
}

/**
 * Holds the resource for each bullet line, plus any string resource IDs
 * that you want to use as placeholders.
 */
internal data class BulletPoint(
    @StringRes val textRes: Int,
    val placeholders: List<Int> = emptyList(),
)

/**
 * @enum InfoScreenTopic
 * @brief Defines the different "info screen" states that can be displayed.
 *
 * This enum is used to categorize the level or mode of an info screen.
 */
internal enum class InfoScreenTopic(
    @StringRes val titleId: Int,
    val bulletPoints:
    List<Pair<BulletPoint, SupportUtils.SumoTopic?>>,
) {
    DEFAULT(
        titleId = R.string.preference_doh_default_protection,
        bulletPoints = listOf(
            BulletPoint(R.string.preference_doh_default_protection_info_1) to null,
            BulletPoint(R.string.preference_doh_default_protection_info_2) to null,
            BulletPoint(
                textRes = R.string.preference_doh_default_protection_info_3,
                placeholders = listOf(
                    R.string.preference_doh_learn_more,
                ),
            ) to SupportUtils.SumoTopic.DNS_OVER_HTTPS_LOCAL_PROVIDER,
            BulletPoint(R.string.preference_doh_default_protection_info_4) to null,
            BulletPoint(
                textRes = R.string.preference_doh_default_protection_info_5,
                placeholders = listOf(
                    R.string.app_name,
                    R.string.preference_doh_learn_more,
                ),
            ) to SupportUtils.SumoTopic.DNS_OVER_HTTPS_NETWORK,
        ),
    ),
    INCREASED(
        titleId = R.string.preference_doh_increased_protection,
        bulletPoints = listOf(
            BulletPoint(R.string.preference_doh_increased_protection_info_1) to null,
            BulletPoint(R.string.preference_doh_increased_protection_info_2) to null,
        ),
    ),
    MAX(
        titleId = R.string.preference_doh_max_protection,
        bulletPoints = listOf(
            BulletPoint(R.string.preference_doh_max_protection_info_1) to null,
            BulletPoint(R.string.preference_doh_max_protection_info_2) to null,
            BulletPoint(R.string.preference_doh_max_protection_info_3) to null,
        ),
    ),
}
