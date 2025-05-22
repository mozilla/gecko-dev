/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import mozilla.appservices.remotesettings.RemoteSettingsException
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.remotesettings.RemoteSettingsService

/**
 * Repository for fetching configuration from Remote Settings.
**/
class RemoteSettingsRepository private constructor() {

    /**
     * Companion object holding utility methods and properties for [RemoteSettingsRepository].
     */
    companion object {
        val logger = Logger("RemoteSettingsRepository")

        /**
         * Fetches records from a Remote Settings collection.
         *
         * @param service The RemoteSettingsService instance to use for fetching data
         * @param collectionName The name of the collection to fetch records from
         * @return List of RemoteSettingsRecord objects or null if fetching fails
         */
        fun fetchRemoteResponse(
            service: RemoteSettingsService,
            collectionName: String,
        ): List<RemoteSettingsRecord>? {
            return try {
                val client = service.remoteSettingsService.makeClient(collectionName)
                return client.getRecords()
            } catch (e: RemoteSettingsException) {
                logger.error("Remote Settings Exception", e)
                emptyList()
            } catch (e: IllegalStateException) {
                logger.error("Illegal State Exception", e)
                emptyList()
            }
        }
    }
}
