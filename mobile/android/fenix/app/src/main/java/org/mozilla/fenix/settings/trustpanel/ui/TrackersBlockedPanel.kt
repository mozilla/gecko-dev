/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.annotation.LightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.MenuGroup
import org.mozilla.fenix.components.menu.compose.MenuItem
import org.mozilla.fenix.components.menu.compose.MenuScaffold
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.trackingprotection.TrackerBuckets
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory

internal const val TRACKERS_PANEL_ROUTE = "trackers_panel"

@Composable
internal fun TrackersBlockedPanel(
    title: String,
    numberOfTrackersBlocked: Int,
    bucketedTrackers: TrackerBuckets,
    onTrackerCategoryClick: (TrackingProtectionCategory) -> Unit,
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
        Column {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = stringResource(
                        R.string.trackers_blocked_panel_total_num_trackers_blocked,
                        numberOfTrackersBlocked,
                    ),
                    modifier = Modifier.weight(1f),
                    color = FirefoxTheme.colors.textAccent,
                    style = FirefoxTheme.typography.headline8,
                )
            }

            Spacer(modifier = Modifier.height(4.dp))

            MenuGroup {
                TrackingProtectionCategory.entries
                    .filter { bucketedTrackers.get(it, true).isNotEmpty() }
                    .forEachIndexed { index, trackingProtectionCategory ->
                        if (index != 0) { Divider(color = FirefoxTheme.colors.borderSecondary) }

                        MenuItem(
                            label = stringResource(
                                R.string.trackers_blocked_panel_categorical_num_trackers_blocked,
                                stringResource(trackingProtectionCategory.title),
                                bucketedTrackers.get(trackingProtectionCategory, true).size,
                            ),
                            beforeIconPainter = painterResource(id = trackingProtectionCategory.icon),
                            onClick = { onTrackerCategoryClick(trackingProtectionCategory) },
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
            TrackersBlockedPanel(
                title = "Mozilla",
                numberOfTrackersBlocked = 0,
                bucketedTrackers = TrackerBuckets(),
                onTrackerCategoryClick = {},
                onBackButtonClick = {},
            )
        }
    }
}
