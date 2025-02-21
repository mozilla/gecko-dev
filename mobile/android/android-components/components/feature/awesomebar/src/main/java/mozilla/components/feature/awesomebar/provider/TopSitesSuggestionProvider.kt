/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.top.sites.DefaultTopSitesStorage
import mozilla.components.feature.top.sites.TopSite
import java.lang.Integer.MAX_VALUE
import java.util.UUID

/**
 * Return 4 top sites by default.
 */
const val DEFAULT_TOP_SITE_LIMIT = 4

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions from [DefaultTopSitesStorage].
 *
 * @param topSitesStorage an instance of [DefaultTopSitesStorage] used to query top sites.
 * @param loadUrlUseCase the use case invoked to load the url when the user clicks on the suggestion.
 * @param icons optional instance of [BrowserIcons] to load favicons for top site URLs.
 * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * first suggestion URL.
 * @param maxNumberOfSuggestions optional parameter to specify the maximum number of returned suggestions,
 * defaults to [DEFAULT_TOP_SITE_LIMIT].
 * @param suggestionsHeader optional parameter to specify if the suggestion should have a header.
 * @param topSitesFilter optional callback to filter the top sites obtained from [topSitesStorage]
 */
class TopSitesSuggestionProvider(
    private val topSitesStorage: DefaultTopSitesStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    internal val engine: Engine? = null,
    private val maxNumberOfSuggestions: Int = DEFAULT_TOP_SITE_LIMIT,
    private val suggestionsHeader: String? = null,
    private val topSitesFilter: (List<TopSite>) -> List<TopSite> = { topSites ->
        topSites.filter { it is TopSite.Frecent || it is TopSite.Pinned }
    },
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override fun groupTitle(): String? {
        return suggestionsHeader
    }

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isNotEmpty()) {
            return emptyList()
        }

        val suggestions = topSitesFilter(topSitesStorage.cachedTopSites)
            .take(maxNumberOfSuggestions)

        suggestions.firstOrNull()?.url?.let { url -> engine?.speculativeConnect(url) }

        return suggestions.toAwesomebarSuggestions(this, icons, loadUrlUseCase)
    }
}

internal suspend fun Iterable<TopSite>.toAwesomebarSuggestions(
    provider: AwesomeBar.SuggestionProvider,
    icons: BrowserIcons?,
    loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
): List<AwesomeBar.Suggestion> {
    val iconRequests = this.map { icons?.loadIcon(IconRequest(url = it.url, waitOnNetworkLoad = false)) }
    return this.withIndex().zip(iconRequests) { (index, result), icon ->
        AwesomeBar.Suggestion(
            provider = provider,
            icon = icon?.await()?.bitmap,
            flags = setOf(AwesomeBar.Suggestion.Flag.HISTORY),
            title = result.title,
            editSuggestion = null,
            onSuggestionClicked = { loadUrlUseCase(result.url) },
            score = MAX_VALUE - index,
        )
    }
}
