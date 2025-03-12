/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.icons

/**
 * Data class representing Search Config Icons from Remote Settings.
 */
data class SearchConfigIconsModel(
    val schema: Long,
    val imageSize: Int,
    val attachment: AttachmentModel?,
    val engineIdentifier: List<String>,
    val filterExpression: String,
)

/**
 * Data class representing an Attachment from Remote Settings.
 */
data class AttachmentModel(
    val filename: String,
    val mimetype: String,
    val location: String,
    val hash: String,
    val size: UInt,
)
