/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.fake

import com.google.firebase.util.nextAlphanumericString
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.home.privatebrowsing.interactor.PrivateBrowsingInteractor
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.home.recentsyncedtabs.interactor.RecentSyncedTabInteractor
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import kotlin.random.Random

/**
 * Utils for building fake data objects for use with compose previews.
 */
internal object FakeHomepagePreview {
    private val random = Random(seed = 1)

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
                            id = randomString(),
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

    private const val URL = "mozilla.com"

    private fun randomLong() = random.nextLong()

    private fun randomString(length: Int = random.nextInt(from = 3, until = 11)) =
        random.nextAlphanumericString(
            length = length,
        )
}
