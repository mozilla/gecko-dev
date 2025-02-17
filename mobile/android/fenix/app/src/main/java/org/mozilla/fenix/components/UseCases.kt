/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.StrictMode
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.accounts.push.CloseTabsUseCases
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SettingsUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.syncedtabs.commands.SyncedTabsCommands
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.TopSitesStorage
import mozilla.components.feature.top.sites.TopSitesUseCases
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.LocaleUseCases
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.home.mars.MARSUseCases
import org.mozilla.fenix.perf.StrictModeManager
import org.mozilla.fenix.perf.lazyMonitored
import org.mozilla.fenix.wallpapers.WallpapersUseCases

/**
 * Component group for all use cases. Use cases are provided by feature
 * modules and can be triggered by UI interactions.
 */
@Suppress("LongParameterList")
class UseCases(
    private val context: Context,
    private val engine: Lazy<Engine>,
    private val store: Lazy<BrowserStore>,
    private val shortcutManager: Lazy<WebAppShortcutManager>,
    private val topSitesStorage: Lazy<TopSitesStorage>,
    private val bookmarksStorage: Lazy<BookmarksStorage>,
    private val historyStorage: Lazy<HistoryStorage>,
    private val syncedTabsCommands: Lazy<SyncedTabsCommands>,
    appStore: Lazy<AppStore>,
    client: Lazy<Client>,
    strictMode: Lazy<StrictModeManager>,
) {
    /**
     * Use cases that provide engine interactions for a given browser session.
     */
    val sessionUseCases by lazyMonitored { SessionUseCases(store.value) }

    /**
     * Use cases that provide tab management.
     */
    val tabsUseCases: TabsUseCases by lazyMonitored { TabsUseCases(store.value) }

    /**
     * Use cases for managing custom tabs.
     */
    val customTabsUseCases: CustomTabsUseCases by lazyMonitored {
        CustomTabsUseCases(store.value, sessionUseCases.loadUrl)
    }

    /**
     * Use cases that provide search engine integration.
     */
    val searchUseCases by lazyMonitored {
        SearchUseCases(
            store.value,
            tabsUseCases,
            sessionUseCases,
        )
    }

    /**
     * Use cases that provide settings management.
     */
    val settingsUseCases by lazyMonitored { SettingsUseCases(engine.value, store.value) }

    val appLinksUseCases by lazyMonitored { AppLinksUseCases(context.applicationContext) }

    val webAppUseCases by lazyMonitored {
        WebAppUseCases(context, store.value, shortcutManager.value)
    }

    val downloadUseCases by lazyMonitored { DownloadsUseCases(store.value) }

    val contextMenuUseCases by lazyMonitored { ContextMenuUseCases(store.value) }

    val trackingProtectionUseCases by lazyMonitored { TrackingProtectionUseCases(store.value, engine.value) }

    /**
     * Use cases that provide top sites management.
     */
    val topSitesUseCase by lazyMonitored { TopSitesUseCases(topSitesStorage.value) }

    /**
     * Use cases that handle locale management.
     */
    val localeUseCases by lazyMonitored { LocaleUseCases(store.value) }

    /**
     * Use cases that provide bookmark management.
     */
    val bookmarksUseCases by lazyMonitored { BookmarksUseCase(bookmarksStorage.value, historyStorage.value) }

    val wallpaperUseCases by lazyMonitored {
        // Required to even access context.filesDir property and to retrieve current locale
        val (rootStorageDirectory, currentLocale) = strictMode.value.resetAfter(StrictMode.allowThreadDiskReads()) {
            val rootStorageDirectory = context.filesDir
            val currentLocale = LocaleManager.getCurrentLocale(context)?.toLanguageTag()
                ?: LocaleManager.getSystemDefault().toLanguageTag()
            rootStorageDirectory to currentLocale
        }
        WallpapersUseCases(context, appStore.value, client.value, rootStorageDirectory, currentLocale)
    }

    val closeSyncedTabsUseCases by lazyMonitored { CloseTabsUseCases(syncedTabsCommands.value) }

    val marsUseCases by lazyMonitored { MARSUseCases(client.value) }
}
