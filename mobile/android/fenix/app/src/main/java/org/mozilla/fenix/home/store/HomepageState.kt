/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.store

import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.shouldShowRecentTabs
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.topsites.TopSiteColors
import org.mozilla.fenix.utils.Settings

/**
 * State object that describes the homepage
 */
internal sealed class HomepageState {

    /**
     * State type corresponding with private browsing mode
     *
     * @property feltPrivateBrowsingEnabled whether felt private browsing is enabled
     */
    internal data class Private(
        val feltPrivateBrowsingEnabled: Boolean,
    ) : HomepageState()

    /**
     * State corresponding with the homepage when firefox is not in private browsing mode
     *
     * @property showTopSites Whether to show top sites or not.
     * @property topSiteColors The color set defined by [TopSiteColors] used to style a top site.
     * @property topSites List of [TopSite] to display.
     * @property showRecentTabs Whether to show recent tabs or not.
     * @property recentTabs List of [RecentTab] to display.
     * @property cardBackgroundColor Background color for card items.
     */
    internal data class Normal(
        val showTopSites: Boolean,
        val topSiteColors: TopSiteColors,
        val topSites: List<TopSite>,
        val showRecentTabs: Boolean,
        val recentTabs: List<RecentTab>,
        val cardBackgroundColor: Color,
    ) : HomepageState()

    companion object {

        /**
         * Builds a new [HomepageState] from the current [AppState] and [Settings]
         *
         * @param appState State to build the [HomepageState] from.
         * @param settings Settings corresponding to how the homepage should be displayed.
         */
        @Composable
        internal fun build(
            appState: AppState,
            settings: Settings,
        ): HomepageState {
            return with(appState) {
                if (mode.isPrivate) {
                    Private(
                        feltPrivateBrowsingEnabled = settings.feltPrivateBrowsingEnabled,
                    )
                } else {
                    Normal(
                        showTopSites = settings.showTopSitesFeature && topSites.isNotEmpty(),
                        topSiteColors = TopSiteColors.colors(wallpaperState = wallpaperState),
                        topSites = topSites,
                        showRecentTabs = shouldShowRecentTabs(settings),
                        recentTabs = recentTabs,
                        cardBackgroundColor = wallpaperState.cardBackgroundColor,
                    )
                }
            }
        }
    }
}
