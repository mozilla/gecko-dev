/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.History
import org.mozilla.fenix.GleanMetrics.RecentlyVisitedHomepage
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.home.HomeSectionHeader
import org.mozilla.fenix.home.bookmarks.Bookmark
import org.mozilla.fenix.home.bookmarks.interactor.BookmarksInteractor
import org.mozilla.fenix.home.bookmarks.view.Bookmarks
import org.mozilla.fenix.home.bookmarks.view.BookmarksMenuItem
import org.mozilla.fenix.home.fake.FakeHomepagePreview
import org.mozilla.fenix.home.privatebrowsing.interactor.PrivateBrowsingInteractor
import org.mozilla.fenix.home.recentsyncedtabs.interactor.RecentSyncedTabInteractor
import org.mozilla.fenix.home.recentsyncedtabs.view.RecentSyncedTab
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.home.recenttabs.view.RecentTabMenuItem
import org.mozilla.fenix.home.recenttabs.view.RecentTabs
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.home.recentvisits.interactor.RecentVisitsInteractor
import org.mozilla.fenix.home.recentvisits.view.RecentVisitMenuItem
import org.mozilla.fenix.home.recentvisits.view.RecentlyVisited
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.FeltPrivacyModeInfoCard
import org.mozilla.fenix.home.sessioncontrol.viewholders.PrivateBrowsingDescription
import org.mozilla.fenix.home.store.HomepageState
import org.mozilla.fenix.home.topsites.TopSiteColors
import org.mozilla.fenix.home.topsites.TopSites
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.WallpaperState

/**
 * Top level composable for the homepage.
 *
 * @param state State representing the homepage.
 * @param privateBrowsingInteractor for interactions in private browsing mode.
 * @param topSiteInteractor For interactions with the top sites UI.
 * @param recentTabInteractor For interactions with the recent tab UI.
 * @param recentSyncedTabInteractor For interactions with the recent synced tab UI.
 * @param bookmarksInteractor For interactions with the bookmarks UI.
 * @param recentVisitsInteractor For interactions with the recent visits UI.
 * @param onTopSitesItemBound Invoked during the composition of a top site item.
 */
@Suppress("LongParameterList")
@Composable
internal fun Homepage(
    state: HomepageState,
    privateBrowsingInteractor: PrivateBrowsingInteractor,
    topSiteInteractor: TopSiteInteractor,
    recentTabInteractor: RecentTabInteractor,
    recentSyncedTabInteractor: RecentSyncedTabInteractor,
    bookmarksInteractor: BookmarksInteractor,
    recentVisitsInteractor: RecentVisitsInteractor,
    onTopSitesItemBound: () -> Unit,
) {
    Column(modifier = Modifier.padding(horizontal = 16.dp)) {
        with(state) {
            when (this) {
                is HomepageState.Private -> {
                    if (feltPrivateBrowsingEnabled) {
                        FeltPrivacyModeInfoCard(
                            onLearnMoreClick = privateBrowsingInteractor::onLearnMoreClicked,
                        )
                    } else {
                        PrivateBrowsingDescription(
                            onLearnMoreClick = privateBrowsingInteractor::onLearnMoreClicked,
                        )
                    }
                }

                is HomepageState.Normal -> {
                    if (showTopSites) {
                        TopSites(
                            topSites = topSites,
                            topSiteColors = topSiteColors,
                            interactor = topSiteInteractor,
                            onTopSitesItemBound = onTopSitesItemBound,
                        )
                    }

                    if (showRecentTabs) {
                        RecentTabsSection(
                            interactor = recentTabInteractor,
                            cardBackgroundColor = cardBackgroundColor,
                            recentTabs = recentTabs,
                        )

                        if (showRecentSyncedTab) {
                            Spacer(modifier = Modifier.height(8.dp))

                            RecentSyncedTab(
                                tab = syncedTab,
                                backgroundColor = cardBackgroundColor,
                                buttonBackgroundColor = buttonBackgroundColor,
                                buttonTextColor = buttonTextColor,
                                onRecentSyncedTabClick = recentSyncedTabInteractor::onRecentSyncedTabClicked,
                                onSeeAllSyncedTabsButtonClick = recentSyncedTabInteractor::onSyncedTabShowAllClicked,
                                onRemoveSyncedTab = recentSyncedTabInteractor::onRemovedRecentSyncedTab,
                            )
                        }
                    }

                    if (showBookmarks) {
                        BookmarksSection(
                            bookmarks = bookmarks,
                            cardBackgroundColor = cardBackgroundColor,
                            interactor = bookmarksInteractor,
                        )
                    }

                    if (showRecentlyVisited) {
                        RecentlyVisitedSection(
                            recentVisits = recentlyVisited,
                            cardBackgroundColor = cardBackgroundColor,
                            interactor = recentVisitsInteractor,
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun RecentTabsSection(
    interactor: RecentTabInteractor,
    cardBackgroundColor: Color,
    recentTabs: List<RecentTab>,
) {
    Spacer(modifier = Modifier.height(40.dp))

    HomeSectionHeader(
        headerText = stringResource(R.string.recent_tabs_header),
        description = stringResource(R.string.recent_tabs_show_all_content_description_2),
        onShowAllClick = interactor::onRecentTabShowAllClicked,
    )

    Spacer(Modifier.height(16.dp))

    RecentTabs(
        recentTabs = recentTabs,
        backgroundColor = cardBackgroundColor,
        onRecentTabClick = { interactor.onRecentTabClicked(it) },
        menuItems = listOf(
            RecentTabMenuItem(
                title = stringResource(id = R.string.recent_tab_menu_item_remove),
                onClick = interactor::onRemoveRecentTab,
            ),
        ),
    )
}

@Composable
private fun BookmarksSection(
    bookmarks: List<Bookmark>,
    cardBackgroundColor: Color,
    interactor: BookmarksInteractor,
) {
    Spacer(modifier = Modifier.height(40.dp))

    HomeSectionHeader(
        headerText = stringResource(R.string.home_bookmarks_title),
        description = stringResource(R.string.home_bookmarks_show_all_content_description),
        onShowAllClick = interactor::onShowAllBookmarksClicked,
    )

    Spacer(Modifier.height(16.dp))

    Bookmarks(
        bookmarks = bookmarks,
        menuItems = listOf(
            BookmarksMenuItem(
                stringResource(id = R.string.home_bookmarks_menu_item_remove),
                onClick = interactor::onBookmarkRemoved,
            ),
        ),
        backgroundColor = cardBackgroundColor,
        onBookmarkClick = interactor::onBookmarkClicked,
    )
}

@Composable
private fun RecentlyVisitedSection(
    recentVisits: List<RecentlyVisitedItem>,
    cardBackgroundColor: Color,
    interactor: RecentVisitsInteractor,
) {
    Spacer(modifier = Modifier.height(40.dp))

    HomeSectionHeader(
        headerText = stringResource(R.string.history_metadata_header_2),
        description = stringResource(R.string.past_explorations_show_all_content_description_2),
        onShowAllClick = interactor::onHistoryShowAllClicked,
    )

    Spacer(Modifier.height(16.dp))

    RecentlyVisited(
        recentVisits = recentVisits,
        menuItems = listOfNotNull(
            RecentVisitMenuItem(
                title = stringResource(R.string.recently_visited_menu_item_remove),
                onClick = { visit ->
                    when (visit) {
                        is RecentHistoryGroup -> interactor.onRemoveRecentHistoryGroup(visit.title)
                        is RecentHistoryHighlight -> interactor.onRemoveRecentHistoryHighlight(
                            visit.url,
                        )
                    }
                },
            ),
        ),
        backgroundColor = cardBackgroundColor,
        onRecentVisitClick = { recentlyVisitedItem, pageNumber ->
            when (recentlyVisitedItem) {
                is RecentHistoryHighlight -> {
                    RecentlyVisitedHomepage.historyHighlightOpened.record(NoExtras())
                    interactor.onRecentHistoryHighlightClicked(recentlyVisitedItem)
                }
                is RecentHistoryGroup -> {
                    RecentlyVisitedHomepage.searchGroupOpened.record(NoExtras())
                    History.recentSearchesTapped.record(
                        History.RecentSearchesTappedExtra(
                            pageNumber.toString(),
                        ),
                    )
                    interactor.onRecentHistoryGroupClicked(recentlyVisitedItem)
                }
            }
        },
    )
}

@Composable
@LightDarkPreview
private fun HomepagePreview() {
    FirefoxTheme {
        val scrollState = rememberScrollState()

        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(color = FirefoxTheme.colors.layer1)
                .verticalScroll(scrollState),
        ) {
            Homepage(
                HomepageState.Normal(
                    showTopSites = true,
                    topSiteColors = TopSiteColors.colors(),
                    topSites = FakeHomepagePreview.topSites(),
                    showRecentTabs = true,
                    recentTabs = FakeHomepagePreview.recentTabs(),
                    cardBackgroundColor = WallpaperState.default.cardBackgroundColor,
                    buttonTextColor = WallpaperState.default.buttonTextColor,
                    buttonBackgroundColor = WallpaperState.default.buttonBackgroundColor,
                    showRecentSyncedTab = true,
                    syncedTab = FakeHomepagePreview.recentSyncedTab(),
                    showBookmarks = true,
                    bookmarks = FakeHomepagePreview.bookmarks(),
                    showRecentlyVisited = true,
                    recentlyVisited = FakeHomepagePreview.recentHistory(),
                ),
                topSiteInteractor = FakeHomepagePreview.topSitesInteractor,
                privateBrowsingInteractor = FakeHomepagePreview.privateBrowsingInteractor,
                recentTabInteractor = FakeHomepagePreview.recentTabInteractor,
                recentSyncedTabInteractor = FakeHomepagePreview.recentSyncedTabInterator,
                bookmarksInteractor = FakeHomepagePreview.bookmarksInteractor,
                recentVisitsInteractor = FakeHomepagePreview.recentVisitsInteractor,
                onTopSitesItemBound = {},
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun PrivateHomepagePreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(color = FirefoxTheme.colors.layer1),
        ) {
            Homepage(
                HomepageState.Private(
                    feltPrivateBrowsingEnabled = false,
                ),
                topSiteInteractor = FakeHomepagePreview.topSitesInteractor,
                privateBrowsingInteractor = FakeHomepagePreview.privateBrowsingInteractor,
                recentTabInteractor = FakeHomepagePreview.recentTabInteractor,
                recentSyncedTabInteractor = FakeHomepagePreview.recentSyncedTabInterator,
                bookmarksInteractor = FakeHomepagePreview.bookmarksInteractor,
                recentVisitsInteractor = FakeHomepagePreview.recentVisitsInteractor,
                onTopSitesItemBound = {},
            )
        }
    }
}
