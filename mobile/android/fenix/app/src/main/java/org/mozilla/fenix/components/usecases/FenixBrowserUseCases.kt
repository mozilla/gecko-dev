/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.usecases

import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl

/**
 * Use cases for handling loading a URL and performing a search.
 *
 * @param addNewTabUseCase [TabsUseCases.AddNewTabUseCase] used for adding new tabs.
 * @param loadUrlUseCase [SessionUseCases.DefaultLoadUrlUseCase] used for loading a URL.
 * @param searchUseCases [SearchUseCases] used for performing a search.
 * @param homepageTitle The title of the new homepage tab.
 * @param profiler [Profiler] used to add profiler markers.
 */
class FenixBrowserUseCases(
    private val addNewTabUseCase: TabsUseCases.AddNewTabUseCase,
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase,
    private val searchUseCases: SearchUseCases,
    private val homepageTitle: String,
    private val profiler: Profiler?,
) {
    /**
     * Loads a URL or performs a search depending on the value of [searchTermOrURL].
     *
     * @param searchTermOrURL The entered search term to search or URL to be loaded.
     * @param newTab Whether or not to load the URL in a new tab.
     * @param private Whether or not the tab should be private.
     * @param forceSearch Whether or not to force performing a search.
     * @param searchEngine Optional [SearchEngine] to use when performing a search.
     * @param flags Flags that will be used when loading the URL (not applied to searches).
     * @param historyMetadata The [HistoryMetadataKey] of the new tab in case this tab
     * was opened from history.
     * @param additionalHeaders The extra headers to use when loading the URL.
     */
    fun loadUrlOrSearch(
        searchTermOrURL: String,
        newTab: Boolean,
        private: Boolean,
        forceSearch: Boolean = false,
        searchEngine: SearchEngine? = null,
        flags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
        historyMetadata: HistoryMetadataKey? = null,
        additionalHeaders: Map<String, String>? = null,
    ) {
        val startTime = profiler?.getProfilerTime()

        // In situations where we want to perform a search but have no search engine (e.g. the user
        // has removed all of them, or we couldn't load any) we will pass searchTermOrURL to Gecko
        // and let it try to load whatever was entered.
        if (searchEngine == null || (!forceSearch && searchTermOrURL.isUrl())) {
            if (newTab) {
                addNewTabUseCase.invoke(
                    url = searchTermOrURL.toNormalizedUrl(),
                    flags = flags,
                    private = private,
                    historyMetadata = historyMetadata,
                    originalInput = searchTermOrURL,
                )
            } else {
                loadUrlUseCase.invoke(
                    url = searchTermOrURL.toNormalizedUrl(),
                    flags = flags,
                    originalInput = searchTermOrURL,
                )
            }
        } else {
            if (newTab) {
                val searchUseCase = if (private) {
                    searchUseCases.newPrivateTabSearch
                } else {
                    searchUseCases.newTabSearch
                }
                searchUseCase.invoke(
                    searchTerms = searchTermOrURL,
                    source = SessionState.Source.Internal.UserEntered,
                    selected = true,
                    searchEngine = searchEngine,
                    flags = flags,
                    additionalHeaders = additionalHeaders,
                )
            } else {
                searchUseCases.defaultSearch.invoke(
                    searchTerms = searchTermOrURL,
                    searchEngine = searchEngine,
                    flags = flags,
                    additionalHeaders = additionalHeaders,
                )
            }
        }

        if (profiler?.isProfilerActive() == true) {
            // Wrapping the `addMarker` method with `isProfilerActive` even though it's no-op when
            // profiler is not active. That way, `text` argument will not create a string builder
            // all the time.
            profiler.addMarker(
                markerName = "FenixBrowserUseCases.loadUrlOrSearch",
                startTime = startTime,
                text = "newTab: $newTab",
            )
        }
    }

    /**
     * Adds a new homepage ("about:home") tab.
     *
     * @param private Whether or not the new homepage tab should be private.
     * @return The ID of the created tab.
     */
    fun addNewHomepageTab(private: Boolean): String {
        return addNewTabUseCase.invoke(
            url = ABOUT_HOME,
            title = homepageTitle,
            private = private,
        )
    }

    /**
     * Loads the homepage ("about:home").
     */
    fun navigateToHomepage() {
        loadUrlUseCase.invoke(url = ABOUT_HOME)
    }

    /**
     * Contains constants used by [FenixBrowserUseCases].
     */
    companion object {
        const val ABOUT_HOME = "about:home"
    }
}
