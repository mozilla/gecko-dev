/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.icons

import mozilla.components.feature.search.RemoteSettingsRepository
import mozilla.components.support.remotesettings.RemoteSettingsService

internal const val SEARCH_CONFIG_ICONS_COLLECTION_NAME = "search-config-icons"

/**
 * Service for updating search configuration icons from Remote Settings.
 */
class SearchConfigIconsUpdateService {
    private val parser = SearchConfigIconsParser()

    /**
     * Fetches the latest search config icons.
     *
     * @param service The [RemoteSettingsService] to fetch data from.
     * @return List of [SearchConfigIconsModel] objects.
     */
    fun fetchIcons(service: RemoteSettingsService): List<SearchConfigIconsModel> {
        return RemoteSettingsRepository.fetchRemoteResponse(
            service = service,
            collectionName = SEARCH_CONFIG_ICONS_COLLECTION_NAME,
        )?.mapNotNull(parser::parseRecord) ?: emptyList()
    }
}
