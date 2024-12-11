/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.theme.FirefoxTheme

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(12.dp)

/**
 * A menu banner for notifying the user about extensions.
 *
 * @param title The header to be displayed underneath the image.
 * @param description The description to be displayed underneath the header.
 * @param linkText The text to be displayed in the link at the bottom of the banner.
 * @param onClick The action to be executed when the bottom link is clicked.
 */
@Composable
internal fun ExtensionsSubmenuBanner(
    title: String,
    description: String,
    linkText: String,
    onClick: () -> Unit,
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        backgroundColor = FirefoxTheme.colors.layer1,
        border = BorderStroke(
            width = 0.5.dp,
            color = FirefoxTheme.colors.borderPrimary,
        ),
        elevation = 0.dp,
        shape = ROUNDED_CORNER_SHAPE,
    ) {
        if (FirefoxTheme.windowSize.isNotSmall()) {
            Row(
                modifier = Modifier.padding(horizontal = 24.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(24.dp),
            ) {
                Image(
                    modifier = Modifier.size(180.dp),
                    painter = painterResource(id = R.drawable.ic_extensions_onboarding),
                    contentDescription = null,
                )

                ExtensionsSubmenuBannerText(
                    title = title,
                    description = description,
                    linkText = linkText,
                    onClick = onClick,
                )
            }
        } else {
            Column(
                modifier = Modifier.padding(bottom = 24.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Image(
                    modifier = Modifier.size(180.dp),
                    painter = painterResource(id = R.drawable.ic_extensions_onboarding),
                    contentDescription = null,
                )

                Spacer(modifier = Modifier.height(16.dp))

                ExtensionsSubmenuBannerText(
                    title = title,
                    description = description,
                    linkText = linkText,
                    onClick = onClick,
                )
            }
        }
    }
}

@Composable
private fun ExtensionsSubmenuBannerText(
    title: String,
    description: String,
    linkText: String,
    onClick: () -> Unit,
) {
    val isWideScreen = FirefoxTheme.windowSize.isNotSmall()
    Column(
        modifier = if (isWideScreen) Modifier else Modifier.padding(horizontal = 24.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
        horizontalAlignment = if (isWideScreen) Alignment.Start else Alignment.CenterHorizontally,
    ) {
        Text(
            text = title,
            modifier = Modifier.semantics {
                heading()
            },
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline7,
            textAlign = if (!isWideScreen) TextAlign.Center else null,
        )

        Text(
            text = description,
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
            textAlign = if (!isWideScreen) TextAlign.Center else null,
        )

        LinkText(
            text = linkText,
            linkTextStates = listOf(
                LinkTextState(
                    text = linkText,
                    url = "",
                    onClick = { onClick() },
                ),
            ),
            linkTextColor = FirefoxTheme.colors.textAccent,
            linkTextDecoration = TextDecoration.Underline,
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun ExtensionsSubmenuBannerPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3)
                .padding(16.dp),
        ) {
            ExtensionsSubmenuBanner(
                title = stringResource(
                    R.string.browser_menu_extensions_banner_onboarding_header,
                    stringResource(R.string.app_name),
                ),
                description = stringResource(
                    R.string.browser_menu_extensions_banner_onboarding_body,
                    stringResource(R.string.app_name),
                ),
                linkText = stringResource(R.string.browser_menu_extensions_banner_learn_more),
                onClick = {},
            )
        }
    }
}
