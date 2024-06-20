/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import kotlinx.serialization.Serializable
import mozilla.components.concept.base.crash.Breadcrumb.Level
import mozilla.components.concept.base.crash.Breadcrumb.Type
import java.text.DateFormat
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

/**
 * Represents a single crash breadcrumb.
 */
@Serializable
internal data class Breadcrumb(
    /**
     * Date of the crash breadcrumb.
     */
    val timestamp: String = "",

    /**
     * Message of the crash breadcrumb.
     */
    val message: String = "",

    /**
     * Category of the crash breadcrumb.
     */
    val category: String = "",

    /**
     * Level of the crash breadcrumb.
     */
    val level: String = Level.DEBUG.value,

    /**
     * Type of the crash breadcrumb.
     */
    val type: String = Type.DEFAULT.value,

    /**
     * Data related to the crash breadcrumb.
     */
    val data: Map<String, String> = emptyMap(),
)

internal fun Breadcrumb.toBreadcrumb(): mozilla.components.concept.base.crash.Breadcrumb =
    mozilla.components.concept.base.crash.Breadcrumb(
        message = message,
        data = data,
        category = category,
        level = Level.entries.firstOrNull { it.value == level } ?: Level.DEBUG,
        type = Type.entries.firstOrNull { it.value == type } ?: Type.DEFAULT,
        date = jsonDateFormat.parse(timestamp) ?: Date(),
    )

internal fun mozilla.components.concept.base.crash.Breadcrumb.toBreadcrumb() = Breadcrumb(
    message = message,
    data = data,
    category = category,
    level = level.value,
    type = type.value,
    timestamp = jsonDateFormat.format(date),
)

private val jsonDateFormat: DateFormat
    get() = SimpleDateFormat(DATE_FORMAT_PATTERN, Locale.US).apply {
        timeZone = TimeZone.getTimeZone("GMT")
    }

private const val DATE_FORMAT_PATTERN = "yyyy-MM-dd'T'HH:mm:ss"
