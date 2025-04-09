/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.icons

import mozilla.appservices.remotesettings.Attachment
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.toList
import org.json.JSONException

/**
 * Parser for search engine icons configuration from Remote Settings records.
 * Converts raw records into structured model objects.
 */
class SearchConfigIconsParser {
    private val logger = Logger("SearchConfigIconsParser")

    /**
     * Parses a Remote Settings record into a SearchConfigIconsModel.
     *
     * @param record The RemoteSettingsRecord to parse
     * @return Parsed SearchConfigIconsModel or null if parsing fails
     */
    fun parseRecord(record: RemoteSettingsRecord): SearchConfigIconsModel? =
        try {
            SearchConfigIconsModel(
                schema = record.fields.getLong("schema"),
                imageSize = record.fields.getInt("imageSize"),
                attachment = record.attachment?.let { parseAttachment(it) },
                engineIdentifier = record.fields.getJSONArray("engineIdentifiers").toList(),
                filterExpression = record.fields.optString("filter_expression"),
            )
        } catch (e: JSONException) {
            logger.error("JSONException while trying to parse search config icons", e)
            null
        }

    private fun parseAttachment(attachment: Attachment): AttachmentModel? =
        try {
            AttachmentModel(
                filename = attachment.filename,
                mimetype = attachment.mimetype,
                location = attachment.location,
                hash = attachment.hash,
                size = attachment.size.toUInt(),
            )
        } catch (e: JSONException) {
            logger.error("JSONException while trying to parse attachment", e)
            null
        }
}
