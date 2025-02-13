/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.MenuScaffold
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.trackingprotection.TrackerBuckets
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory

internal const val TRACKER_CATEGORY_DETAILS_PANEL_ROUTE = "tracker_category_details_panel"

@Composable
internal fun TrackerCategoryDetailsPanel(
    title: String,
    isTotalCookieProtectionEnabled: Boolean,
    detailedTrackerCategory: TrackingProtectionCategory?,
    bucketedTrackers: TrackerBuckets,
    onBackButtonClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = title,
                onClick = onBackButtonClick,
            )
        },
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp),
        ) {
            val trackerCategoryTitle: String
            val trackerCategoryDescription: String

            if (detailedTrackerCategory == TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES &&
                isTotalCookieProtectionEnabled
            ) {
                trackerCategoryTitle = stringResource(id = R.string.etp_cookies_title_2)
                trackerCategoryDescription = stringResource(id = R.string.etp_cookies_description_2)
            } else if (detailedTrackerCategory != null) {
                trackerCategoryTitle = stringResource(id = detailedTrackerCategory.title)
                trackerCategoryDescription = stringResource(id = detailedTrackerCategory.description)
            } else {
                trackerCategoryTitle = ""
                trackerCategoryDescription = ""
            }

            Text(
                text = trackerCategoryTitle,
                color = FirefoxTheme.colors.textAccent,
                style = FirefoxTheme.typography.headline8,
            )

            Spacer(modifier = Modifier.height(8.dp))

            Text(
                text = trackerCategoryDescription,
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.body2,
            )

            Spacer(modifier = Modifier.height(16.dp))

            Text(
                text = stringResource(id = R.string.enhanced_tracking_protection_blocked),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline8,
            )

            Spacer(modifier = Modifier.height(8.dp))

            detailedTrackerCategory?.let {
                bucketedTrackers.get(detailedTrackerCategory, true).forEach {
                    Text(
                        text = it.url.tryGetHostFromUrl(),
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.body2,
                    )
                }
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun TrackersBlockedPanelPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            TrackerCategoryDetailsPanel(
                title = "Mozilla",
                isTotalCookieProtectionEnabled = true,
                detailedTrackerCategory = TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES,
                bucketedTrackers = TrackerBuckets(),
                onBackButtonClick = {},
            )
        }
    }
}
