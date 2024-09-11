/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import kotlinx.serialization.json.Json
import mozilla.components.lib.crash.Crash
import mozilla.components.support.base.ext.getStacktraceAsString

/**
 * Database entity modeling a crash that has happened.
 */
@Entity(
    tableName = "crashes",
)
internal data class CrashEntity(
    /* shared fields- both uncaught exception and native crashes */

    /**
     * Type of crash- either UNCAUGHT or NATIVE
     */
    @ColumnInfo(name = "crashType", defaultValue = "UNCAUGHT")
    var crashType: CrashType,

    /**
     * Generated UUID for this crash.
     */
    @PrimaryKey
    @ColumnInfo(name = "uuid")
    var uuid: String,

    /**
     * Runtime tags that should be attached to any report associated with this crash.
     */
    @ColumnInfo(name = "runtime_tags", defaultValue = "{}")
    var runtimeTags: Map<String, String>,

    /**
     * List of breadcrumbs to send with the crash report.
     */
    @ColumnInfo(name = "breadcrumbs", defaultValue = "null")
    var breadcrumbs: List<String>? = emptyList(),

    /**
     * Timestamp (in milliseconds) of when the crash happened.
     */
    @ColumnInfo(name = "created_at")
    var createdAt: Long,

    /* Uncaught exception crash fields */

    /**
     * The stacktrace of the crash (if this crash was caused by an exception/throwable): otherwise
     * a string describing the type of crash.
     */
    @ColumnInfo(name = "stacktrace")
    var stacktrace: String,

    /* Native crash fields */

    /**
     * Path to a Breakpad minidump file containing information about the crash.
     */
    @ColumnInfo(name = "minidumpPath", defaultValue = "null")
    var minidumpPath: String?,

    /**
     * The type of process the crash occurred in. Affects whether or not the crash is fatal
     * or whether the application can recover from it.
     */
    @ColumnInfo(name = "processType", defaultValue = "null")
    var processType: String?,

    /**
     * Indicating whether or not the crash dump was successfully retrieved. If this is false, the
     * dump file may be corrupted or incomplete.
     */
    @ColumnInfo(name = "minidumpSuccess", defaultValue = "null")
    var minidumpSuccess: Boolean?,

    /**
     * Path to a file containing extra metadata about the crash. The file contains key-value pairs
     * in the form `Key=Value`. Be aware, it may contain sensitive data such as the URI that was
     * loaded at the time of the crash.
     */
    @ColumnInfo(name = "extrasPath", defaultValue = "null")
    var extrasPath: String?,

    /**
     * The type of child process (when available).
     */
    @ColumnInfo(name = "remoteType", defaultValue = "null")
    var remoteType: String?,
)

internal fun Crash.toEntity(): CrashEntity {
    return when (this) {
        is Crash.NativeCodeCrash -> toEntity()
        is Crash.UncaughtExceptionCrash -> toEntity()
    }
}

private fun Crash.NativeCodeCrash.toEntity(): CrashEntity =
    CrashEntity(
        crashType = CrashType.NATIVE,
        uuid = uuid,
        runtimeTags = runtimeTags,
        breadcrumbs = breadcrumbs.map { Json.encodeToString(Breadcrumb.serializer(), it.toBreadcrumb()) },
        createdAt = timestamp,
        stacktrace = "<native crash>",
        minidumpPath = minidumpPath,
        minidumpSuccess = minidumpSuccess,
        processType = processType,
        extrasPath = extrasPath,
        remoteType = remoteType,
    )

private fun Crash.UncaughtExceptionCrash.toEntity(): CrashEntity =
    CrashEntity(
        crashType = CrashType.UNCAUGHT,
        uuid = uuid,
        runtimeTags = runtimeTags,
        breadcrumbs = breadcrumbs.map { Json.encodeToString(Breadcrumb.serializer(), it.toBreadcrumb()) },
        createdAt = timestamp,
        stacktrace = throwable.getStacktraceAsString(),
        minidumpPath = null,
        minidumpSuccess = null,
        processType = null,
        extrasPath = null,
        remoteType = null,
    )

internal enum class CrashType {
    NATIVE, UNCAUGHT
}
