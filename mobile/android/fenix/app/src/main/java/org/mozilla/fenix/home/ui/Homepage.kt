/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.home.HomeSectionHeader
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.shouldShowRecentTabs
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.home.recenttabs.view.RecentTabMenuItem
import org.mozilla.fenix.home.recenttabs.view.RecentTabs
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.FeltPrivacyModeInfoCard
import org.mozilla.fenix.home.sessioncontrol.viewholders.PrivateBrowsingDescription
import org.mozilla.fenix.home.topsites.TopSiteColors
import org.mozilla.fenix.home.topsites.TopSites
import org.mozilla.fenix.wallpapers.WallpaperState

/**
 * Top level composable for the homepage.
 *
 * @param interactor The [SessionControlInteractor] for the homepage.
 * @param onTopSitesItemBound Invoked during the composition of a top site item.
 */
@Composable
fun Homepage(
    interactor: SessionControlInteractor,
    onTopSitesItemBound: () -> Unit,
) {
    val appStore = components.appStore
    val settings = LocalContext.current.settings()

    val isPrivateMode by appStore.observeAsState(initialValue = false) { it.mode.isPrivate }
    val topSites by appStore.observeAsState(initialValue = emptyList()) { state ->
        state.topSites
    }
    val wallpaperState by appStore.observeAsState(initialValue = WallpaperState.default) { state ->
        state.wallpaperState
    }

    Column(modifier = Modifier.padding(horizontal = 16.dp)) {
        if (isPrivateMode) {
            if (settings.feltPrivateBrowsingEnabled) {
                FeltPrivacyModeInfoCard(
                    onLearnMoreClick = interactor::onLearnMoreClicked,
                )
            } else {
                PrivateBrowsingDescription(
                    onLearnMoreClick = interactor::onLearnMoreClicked,
                )
            }
        } else {
            val showTopSites = settings.showTopSitesFeature && topSites.isNotEmpty()
            if (showTopSites) {
                TopSites(
                    topSites = topSites,
                    topSiteColors = TopSiteColors.colors(wallpaperState = wallpaperState),
                    onTopSiteClick = { topSite ->
                        interactor.onSelectTopSite(
                            topSite = topSite,
                            position = topSites.indexOf(topSite),
                        )
                    },
                    onTopSiteLongClick = interactor::onTopSiteLongClicked,
                    onOpenInPrivateTabClicked = interactor::onOpenInPrivateTabClicked,
                    onEditTopSiteClicked = interactor::onEditTopSiteClicked,
                    onRemoveTopSiteClicked = interactor::onRemoveTopSiteClicked,
                    onSettingsClicked = interactor::onSettingsClicked,
                    onSponsorPrivacyClicked = interactor::onSponsorPrivacyClicked,
                    onTopSitesItemBound = onTopSitesItemBound,
                )
            }

            val showRecentTabs = appStore.state.shouldShowRecentTabs(settings)
            if (showRecentTabs) {
                RecentTabsSection(
                    interactor = interactor,
                    wallpaperState = wallpaperState,
                )
            }
        }
    }
}

@Composable
private fun RecentTabsSection(
    interactor: RecentTabInteractor,
    wallpaperState: WallpaperState,
) {
    val recentTabs = components.appStore.observeAsComposableState { state -> state.recentTabs }

    Spacer(modifier = Modifier.height(40.dp))

    HomeSectionHeader(
        headerText = stringResource(R.string.recent_tabs_header),
        description = stringResource(R.string.recent_tabs_show_all_content_description_2),
        onShowAllClick = {
            interactor.onRecentTabShowAllClicked()
        },
    )

    Spacer(Modifier.height(16.dp))

    RecentTabs(
        recentTabs = recentTabs.value ?: emptyList(),
        backgroundColor = wallpaperState.wallpaperCardColor,
        onRecentTabClick = { interactor.onRecentTabClicked(it) },
        menuItems = listOf(
            RecentTabMenuItem(
                title = stringResource(id = R.string.recent_tab_menu_item_remove),
                onClick = { tab -> interactor.onRemoveRecentTab(tab) },
            ),
        ),
    )
}
