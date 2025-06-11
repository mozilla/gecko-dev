/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * A full-width banner shown in the menu prompting the user to set Firefox as their default browser.
 *
 * The entire banner (icon, illustration, and text) is clickable to launch the
 * system default-browser picker. An “X” icon at the end lets the user permanently dismiss the
 * banner.
 *
 * @param onDismiss Invoked when the user taps the dismiss icon (“X”).
 * @param onClick Invoked when the user taps anywhere else on the banner.
 */
@Composable
fun DefaultBrowserBanner(
    onDismiss: () -> Unit,
    onClick: () -> Unit,
) {
    val appName = LocalContext.current.getString(R.string.app_name)
    val shape = RoundedCornerShape(28.dp)

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clip(shape)
            .clickable(onClick = onClick),
        color = FirefoxTheme.colors.layer3,
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Row(
                modifier = Modifier.clickable(onClick = onClick),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Image(
                    painter = painterResource(id = R.drawable.firefox_as_default_banner_illustration),
                    contentDescription = null,
                )

                Spacer(modifier = Modifier.width(12.dp))

                Column {
                    Text(
                        text = stringResource(
                            id = R.string.browser_menu_default_banner_title,
                            appName,
                        ),
                        style = FirefoxTheme.typography.subtitle1,
                        color = FirefoxTheme.colors.textPrimary,
                    )
                    Text(
                        text = stringResource(id = R.string.browser_menu_default_banner_subtitle),
                        style = FirefoxTheme.typography.caption,
                        color = FirefoxTheme.colors.textSecondary,
                    )
                }
            }

            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_cross_24),
                contentDescription = stringResource(id = R.string.browser_menu_default_banner_dismiss),
                modifier = Modifier
                    .padding(end = 10.dp)
                    .size(24.dp)
                    .clickable(onClick = onDismiss),
                tint = FirefoxTheme.colors.iconSecondary,
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun DefaultBrowserBannerPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .padding(16.dp),
        ) {
            DefaultBrowserBanner(
                onDismiss = {},
                onClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun DefaultBrowserBannerPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .padding(16.dp),
        ) {
            DefaultBrowserBanner(
                onDismiss = {},
                onClick = {},
            )
        }
    }
}
