/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.icons

import mozilla.appservices.remotesettings.RemoteSettingsClient
import mozilla.appservices.remotesettings.RemoteSettingsException
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.components.feature.search.RemoteSettingsRepository
import mozilla.components.support.remotesettings.RemoteSettingsService

internal const val SEARCH_CONFIG_ICONS_COLLECTION_NAME = "search-config-icons"

/**
 * Service for updating search configuration icons from Remote Settings.
 */
class SearchConfigIconsUpdateService(
    private val client: RemoteSettingsClient?,
) {

    /**
     * Fetches the latest search config icons.
     *
     * @param service The [RemoteSettingsService] to fetch data from.
     * @return List of [SearchConfigIconsModel] objects.
     */
    fun fetchIconsRecords(service: RemoteSettingsService): List<RemoteSettingsRecord> {
        return RemoteSettingsRepository.fetchRemoteResponse(
            service = service,
            collectionName = SEARCH_CONFIG_ICONS_COLLECTION_NAME,
            client = client,
        ) ?: emptyList()
    }

    /**
     * Fetches the latest search config icons.
     *
     * @param record The [RemoteSettingsRecord] who's attachment is to be fetched.
=     */
    fun fetchIconAttachment(record: RemoteSettingsRecord?): ByteArray? {
        return record?.let {
            try {
                client?.getAttachment(it)
            } catch (e: RemoteSettingsException) {
                null
            }
        }
    }
}
