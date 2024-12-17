/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.fake

import android.content.Context
import androidx.compose.runtime.Composable
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryShim
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.compose.SelectableChipColors
import org.mozilla.fenix.home.bookmarks.Bookmark
import org.mozilla.fenix.home.bookmarks.interactor.BookmarksInteractor
import org.mozilla.fenix.home.collections.CollectionsState
import org.mozilla.fenix.home.interactor.HomepageInteractor
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketState
import org.mozilla.fenix.home.privatebrowsing.interactor.PrivateBrowsingInteractor
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.home.recentsyncedtabs.interactor.RecentSyncedTabInteractor
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.home.recentvisits.interactor.RecentVisitsInteractor
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.WallpaperState
import java.io.File
import java.util.UUID
import kotlin.random.Random

/**
 * Utils for building fake data objects for use with compose previews.
 */
internal object FakeHomepagePreview {
    private val random = Random(seed = 1)

    val homepageInteractor: HomepageInteractor
        get() = object :
            HomepageInteractor,
            PrivateBrowsingInteractor by privateBrowsingInteractor,
            TopSiteInteractor by topSitesInteractor,
            RecentTabInteractor by recentTabInteractor,
            RecentSyncedTabInteractor by recentSyncedTabInterator,
            BookmarksInteractor by bookmarksInteractor,
            RecentVisitsInteractor by recentVisitsInteractor,
            CollectionInteractor by collectionInteractor {
            override fun reportSessionMetrics(state: AppState) { /* no op */ }

            override fun onPasteAndGo(clipboardText: String) { /* no op */ }

            override fun onPaste(clipboardText: String) { /* no op */ }

            override fun onNavigateSearch() { /* no op */ }

            override fun onMessageClicked(message: Message) { /* no op */ }

            override fun onMessageClosedClicked(message: Message) { /* no op */ }

            override fun openCustomizeHomePage() { /* no op */ }

            override fun onStoryShown(
                storyShown: PocketStory,
                storyPosition: Pair<Int, Int>,
            ) { /* no op */ }

            override fun onStoriesShown(storiesShown: List<PocketStory>) { /* no op */ }

            override fun onCategoryClicked(categoryClicked: PocketRecommendedStoriesCategory) { /* no op */ }

            override fun onStoryClicked(
                storyClicked: PocketStory,
                storyPosition: Pair<Int, Int>,
            ) { /* no op */ }

            override fun onLearnMoreClicked(link: String) { /* no op */ }

            override fun onDiscoverMoreClicked(link: String) { /* no op */ }

            override fun onMenuItemTapped(item: SearchSelectorMenu.Item) { /* no op */ }

            override fun showWallpapersOnboardingDialog(state: WallpaperState): Boolean {
                return false
            }
        }

    internal val privateBrowsingInteractor
        get() = object : PrivateBrowsingInteractor {
            override fun onLearnMoreClicked() { /* no op */ }

            override fun onPrivateModeButtonClicked(newMode: BrowsingMode) { /* no op */ }
        }

    internal val topSitesInteractor
        get() = object : TopSiteInteractor {
            override fun onOpenInPrivateTabClicked(topSite: TopSite) { /* no op */ }

            override fun onEditTopSiteClicked(topSite: TopSite) { /* no op */ }

            override fun onRemoveTopSiteClicked(topSite: TopSite) { /* no op */ }

            override fun onSelectTopSite(topSite: TopSite, position: Int) { /* no op */ }

            override fun onSettingsClicked() { /* no op */ }

            override fun onSponsorPrivacyClicked() { /* no op */ }

            override fun onTopSiteLongClicked(topSite: TopSite) { /* no op */ }
        }

    internal val recentTabInteractor
        get() = object : RecentTabInteractor {
            override fun onRecentTabClicked(tabId: String) { /* no op */ }

            override fun onRecentTabShowAllClicked() { /* no op */ }

            override fun onRemoveRecentTab(tab: RecentTab.Tab) { /* no op */ }
        }

    internal val recentSyncedTabInterator
        get() = object : RecentSyncedTabInteractor {
            override fun onRecentSyncedTabClicked(tab: RecentSyncedTab) { /* no op */ }

            override fun onSyncedTabShowAllClicked() { /* no op */ }

            override fun onRemovedRecentSyncedTab(tab: RecentSyncedTab) { /* no op */ }
        }

    internal val bookmarksInteractor
        get() = object : BookmarksInteractor {
            override fun onBookmarkClicked(bookmark: Bookmark) { /* no op */ }

            override fun onShowAllBookmarksClicked() { /* no op */ }

            override fun onBookmarkRemoved(bookmark: Bookmark) { /* no op */ }
        }

    internal val recentVisitsInteractor
        get() = object : RecentVisitsInteractor {
            override fun onHistoryShowAllClicked() { /* no op */ }

            override fun onRecentHistoryGroupClicked(recentHistoryGroup: RecentHistoryGroup) { /* no op */ }

            override fun onRemoveRecentHistoryGroup(groupTitle: String) { /* no op */ }

            override fun onRecentHistoryHighlightClicked(recentHistoryHighlight: RecentHistoryHighlight) { /* no op */ }

            override fun onRemoveRecentHistoryHighlight(highlightUrl: String) { /* no op */ }
        }

    internal val collectionInteractor
        get() = object : CollectionInteractor {
            override fun onCollectionAddTabTapped(collection: TabCollection) { /* no op */ }

            override fun onCollectionOpenTabClicked(tab: Tab) { /* no op */ }

            override fun onCollectionOpenTabsTapped(collection: TabCollection) { /* no op */ }

            override fun onCollectionRemoveTab(collection: TabCollection, tab: Tab) { /* no op */ }

            override fun onCollectionShareTabsClicked(collection: TabCollection) { /* no op */ }

            override fun onDeleteCollectionTapped(collection: TabCollection) { /* no op */ }

            override fun onRenameCollectionTapped(collection: TabCollection) { /* no op */ }

            override fun onToggleCollectionExpanded(
                collection: TabCollection,
                expand: Boolean,
            ) { /* no op */ }

            override fun onAddTabsToCollectionTapped() { /* no op */ }

            override fun onRemoveCollectionsPlaceholder() { /* no op */ }
        }

    internal fun topSites(
        pinnedCount: Int = 2,
        providedCount: Int = 2,
        defaultCount: Int = 2,
        showPocketTopArticles: Boolean = true,
    ) = mutableListOf<TopSite>().apply {
        repeat(pinnedCount) {
            add(
                TopSite.Pinned(
                    id = randomLong(),
                    title = "Mozilla",
                    url = URL,
                    createdAt = randomLong(),
                ),
            )
        }

        repeat(providedCount) {
            add(
                TopSite.Provided(
                    id = randomLong(),
                    title = "Mozilla",
                    url = URL,
                    clickUrl = URL,
                    imageUrl = URL,
                    impressionUrl = URL,
                    createdAt = randomLong(),
                ),
            )
        }

        repeat(defaultCount) {
            add(
                TopSite.Default(
                    id = randomLong(),
                    title = "Mozilla",
                    url = URL,
                    createdAt = randomLong(),
                ),
            )
        }

        if (showPocketTopArticles) {
            add(
                TopSite.Default(
                    id = null,
                    title = "Top Articles",
                    url = "https://getpocket.com/fenixtoparticles",
                    createdAt = 0L,
                ),
            )
        }
    }

    internal fun recentTabs(tabCount: Int = 2): List<RecentTab.Tab> =
        mutableListOf<RecentTab.Tab>().apply {
            repeat(tabCount) {
                add(
                    RecentTab.Tab(
                        TabSessionState(
                            id = randomId(),
                            content = ContentState(
                                url = URL,
                            ),
                        ),
                    ),
                )
            }
        }

    internal fun recentSyncedTab() =
        RecentSyncedTab(
            deviceDisplayName = "Desktop",
            deviceType = DeviceType.DESKTOP,
            title = "Mozilla",
            url = URL,
            previewImageUrl = null,
        )

    internal fun bookmarks(bookmarkCount: Int = 4) =
        mutableListOf<Bookmark>().apply {
            repeat(bookmarkCount) {
                add(
                    Bookmark(
                        title = "Other Bookmark Title",
                        url = "https://www.example.com",
                        previewImageUrl = null,
                    ),
                )
            }
        }

    internal fun recentHistory(
        historyGroupCount: Int = 1,
        historyHightlightCount: Int = 1,
    ): List<RecentlyVisitedItem> =
        mutableListOf<RecentlyVisitedItem>().apply {
            repeat(historyGroupCount) {
                add(
                    RecentHistoryGroup(title = "running shoes"),
                )
            }

            repeat(historyHightlightCount) {
                add(
                    RecentHistoryHighlight(title = "Mozilla", url = "www.mozilla.com"),
                )
            }
        }

    internal fun collectionState() = CollectionsState.Content(
        collections = listOf(collection(tabs = listOf(tab()))),
        expandedCollections = setOf(),
        showSaveTabsToCollection = true,
    )

    internal fun collection(tabs: List<Tab> = emptyList()): TabCollection {
        return object : TabCollection {
            override val id: Long = 1L
            override val tabs: List<Tab> = tabs
            override val title: String = "Collection 1"

            override fun restore(
                context: Context,
                engine: Engine,
                restoreSessionId: Boolean,
            ): List<RecoverableTab> = emptyList()

            override fun restoreSubset(
                context: Context,
                engine: Engine,
                tabs: List<Tab>,
                restoreSessionId: Boolean,
            ): List<RecoverableTab> = emptyList()
        }
    }

    internal fun tab(): Tab {
        return object : Tab {
            override val id = 2L
            override val title = "Mozilla-Firefox"
            override val url = "https://www.mozilla.org/en-US/firefox/whats-new-in-last-version"

            override fun restore(
                filesDir: File,
                engine: Engine,
                restoreSessionId: Boolean,
            ): RecoverableTab? = null
        }
    }

    @Composable
    internal fun pocketState(limit: Int = 1) = PocketState(
        stories = mutableListOf<PocketStory>().apply {
            for (index in 0 until limit) {
                when (index % 2 == 0) {
                    true -> add(
                        PocketRecommendedStory(
                            title = "This is a ${"very ".repeat(index)} long title",
                            publisher = "Publisher",
                            url = "https://story$index.com",
                            imageUrl = "",
                            timeToRead = index,
                            category = "Category #$index",
                            timesShown = index.toLong(),
                        ),
                    )

                    false -> add(
                        PocketSponsoredStory(
                            id = index,
                            title = "This is a ${"very ".repeat(index)} long title",
                            url = "https://sponsored-story$index.com",
                            imageUrl = "",
                            sponsor = "Mozilla",
                            shim = PocketSponsoredStoryShim("", ""),
                            priority = index,
                            caps = PocketSponsoredStoryCaps(
                                flightCount = index,
                                flightPeriod = index * 2,
                                lifetimeCount = index * 3,
                            ),
                        ),
                    )
                }
            }
        },
        categories = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor"
            .split(" ")
            .map { PocketRecommendedStoriesCategory(it) },
        categoriesSelections = emptyList(),
        showContentRecommendations = false,
        categoryColors = SelectableChipColors.buildColors(),
        textColor = FirefoxTheme.colors.textPrimary,
        linkTextColor = FirefoxTheme.colors.textAccent,
    )

    private const val URL = "mozilla.com"

    private fun randomLong() = random.nextLong()

    private fun randomId() = UUID.randomUUID().toString()
}
