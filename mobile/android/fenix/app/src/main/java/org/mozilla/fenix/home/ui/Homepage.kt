/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.platform.LocalContext
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.components.components
import org.mozilla.fenix.ext.settings
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

    Column {
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
            if (settings.showTopSitesFeature && topSites.isNotEmpty()) {
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
                    onRenameTopSiteClicked = interactor::onRenameTopSiteClicked,
                    onRemoveTopSiteClicked = interactor::onRemoveTopSiteClicked,
                    onSettingsClicked = interactor::onSettingsClicked,
                    onSponsorPrivacyClicked = interactor::onSponsorPrivacyClicked,
                    onTopSitesItemBound = onTopSitesItemBound,
                )
            }
        }
    }
}
