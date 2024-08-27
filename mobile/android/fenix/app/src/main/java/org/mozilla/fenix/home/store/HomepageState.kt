/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.store

import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.shouldShowRecentSyncedTabs
import org.mozilla.fenix.ext.shouldShowRecentTabs
import org.mozilla.fenix.home.bookmarks.Bookmark
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.topsites.TopSiteColors
import org.mozilla.fenix.utils.Settings

/**
 * State object that describes the homepage.
 */
internal sealed class HomepageState {

    /**
     * State type corresponding with private browsing mode.
     *
     * @property feltPrivateBrowsingEnabled Whether felt private browsing is enabled.
     */
    internal data class Private(
        val feltPrivateBrowsingEnabled: Boolean,
    ) : HomepageState()

    /**
     * State corresponding with the homepage in normal browsing mode.
     *
     * @property topSites List of [TopSite] to display.
     * @property recentTabs List of [RecentTab] to display.
     * @property syncedTab The [RecentSyncedTab] to display.
     * @property bookmarks List of [Bookmark] to display.
     * @property showTopSites Whether to show top sites or not.
     * @property showRecentTabs Whether to show recent tabs or not.
     * @property showRecentSyncedTab Whether to show recent synced tab or not.
     * @property showBookmarks Whether to show bookmarks.
     * @property topSiteColors The color set defined by [TopSiteColors] used to style a top site.
     * @property cardBackgroundColor Background color for card items.
     * @property buttonBackgroundColor Background [Color] for buttons.
     * @property buttonTextColor Text [Color] for buttons.
     */
    internal data class Normal(
        val topSites: List<TopSite>,
        val recentTabs: List<RecentTab>,
        val syncedTab: RecentSyncedTab?,
        val bookmarks: List<Bookmark>,
        val showTopSites: Boolean,
        val showRecentTabs: Boolean,
        val showRecentSyncedTab: Boolean,
        val showBookmarks: Boolean,
        val topSiteColors: TopSiteColors,
        val cardBackgroundColor: Color,
        val buttonBackgroundColor: Color,
        val buttonTextColor: Color,
    ) : HomepageState()

    companion object {

        /**
         * Builds a new [HomepageState] from the current [AppState] and [Settings].
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
                        showRecentSyncedTab = shouldShowRecentSyncedTabs(),
                        syncedTab = when (recentSyncedTabState) {
                            RecentSyncedTabState.None,
                            RecentSyncedTabState.Loading,
                            -> null
                            is RecentSyncedTabState.Success -> recentSyncedTabState.tabs.firstOrNull()
                        },
                        buttonBackgroundColor = wallpaperState.buttonBackgroundColor,
                        buttonTextColor = wallpaperState.buttonTextColor,
                        showBookmarks = settings.showBookmarksHomeFeature && bookmarks.isNotEmpty(),
                        bookmarks = bookmarks,
                    )
                }
            }
        }
    }
}
