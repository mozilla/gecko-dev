/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.settings.trustpanel.store.WebsiteInfoState
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

private val ICON_SIZE = 16.dp
private val ICON_PADDING = 8.dp
private val OUTER_ICON_SHAPE = RoundedCornerShape(4.dp)
private val INNER_ICON_SHAPE = RoundedCornerShape(0.dp)

@Composable
internal fun ProtectionPanelHeader(
    icon: Bitmap?,
    websiteInfoState: WebsiteInfoState,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        ProtectionPanelIcon(url = websiteInfoState.websiteUrl, icon = icon)

        Spacer(modifier = Modifier.width(16.dp))

        Column(
            modifier = Modifier
                .weight(1f),
        ) {
            Text(
                text = websiteInfoState.websiteTitle,
                color = FirefoxTheme.colors.textSecondary,
                maxLines = 1,
                style = FirefoxTheme.typography.headline7,
                overflow = TextOverflow.Ellipsis,
            )

            Text(
                text = websiteInfoState.websiteUrl.tryGetHostFromUrl(),
                color = FirefoxTheme.colors.textSecondary,
                maxLines = 1,
                style = FirefoxTheme.typography.caption,
            )
        }
    }
}

@Composable
private fun ProtectionPanelIcon(
    url: String,
    icon: Bitmap?,
) {
    if (icon != null && !icon.isRecycled) {
        Image(
            bitmap = icon.asImageBitmap(),
            contentDescription = null,
            modifier = Modifier
                .background(
                    color = FirefoxTheme.colors.layer2,
                    shape = OUTER_ICON_SHAPE,
                )
                .padding(all = ICON_PADDING)
                .size(ICON_SIZE),
        )
    } else {
        Favicon(
            url = url,
            modifier = Modifier
                .background(
                    color = FirefoxTheme.colors.layer2,
                    shape = OUTER_ICON_SHAPE,
                )
                .padding(all = ICON_PADDING),
            size = ICON_SIZE,
            roundedCornerShape = INNER_ICON_SHAPE,
        )
    }
}

@PreviewLightDark
@Composable
private fun ProtectionPanelHeaderPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1),
        ) {
            ProtectionPanelHeader(
                icon = null,
                websiteInfoState = WebsiteInfoState(
                    isSecured = true,
                    websiteUrl = "https://www.mozilla.org",
                    websiteTitle = "Mozilla",
                    certificateName = "",
                ),
            )
        }
    }
}

@Preview
@Composable
private fun ProtectionPanelHeaderPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            ProtectionPanelHeader(
                icon = null,
                websiteInfoState = WebsiteInfoState(
                    isSecured = false,
                    websiteUrl = "https://www.mozilla.org",
                    websiteTitle = "Mozilla",
                    certificateName = "",
                ),
            )
        }
    }
}
