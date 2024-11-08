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
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import mozilla.components.concept.base.crash.Breadcrumb as CrashBreadcrumb

/**
 * [Throwable] that gets created when the Crash Reporter isn't able to restore the serialized
 * throwable in a CrashEntity.
 */
data class CrashReporterUnableToRestoreException(override var message: String) : Exception()

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

    /**
     * The serialized [Throwable] tht caused the crash.
     */
    @ColumnInfo(name = "throwable")
    val throwableData: ByteArray?,

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

internal fun CrashEntity.deserializeBreadcrumbs(): ArrayList<CrashBreadcrumb> =
    Result.runCatching {
        breadcrumbs
            ?.map { Json.decodeFromString<Breadcrumb>(it).toBreadcrumb() }
            ?.let { ArrayList(it) } ?: arrayListOf()
    }.getOrDefault(arrayListOf())

internal fun CrashEntity.toCrash(): Crash {
    return when (this.crashType) {
        CrashType.NATIVE -> Crash.NativeCodeCrash(
            timestamp = this.createdAt,
            minidumpPath = this.minidumpPath,
            extrasPath = this.extrasPath,
            processType = this.processType,
            breadcrumbs = deserializeBreadcrumbs(),
            remoteType = this.remoteType,
            runtimeTags = this.runtimeTags,
            uuid = this.uuid,
        )
        CrashType.UNCAUGHT -> Crash.UncaughtExceptionCrash(
            timestamp = this.createdAt,
            throwable = deserializeThrowable(),
            breadcrumbs = deserializeBreadcrumbs(),
            runtimeTags = runtimeTags,
            uuid = this.uuid,
        )
    }
}

internal fun CrashEntity.restoreArchiveError(message: String): Throwable {
    val lines = stacktrace.trim().lines()
    if (lines.isEmpty()) return CrashReporterUnableToRestoreException("Stack trace string is empty or invalid.")

    val throwable = CrashReporterUnableToRestoreException(message)

    // this Regex will take a line of a stack trace and pulls the class / method, file name and line number.
    // for example, if we're given:
    // at androidx.recyclerview.widget.LinearLayoutManager.fill(LinearLayoutManager.java:49)
    // we will get ("androidx.recyclerview.widget.LinearLayoutManager.fill", "LinearLayoutManager.java", 49).
    val regex = Regex("""\s*at (.+)\((.+):(\d+)\)""")

    val stackTraceElements = lines.drop(1).mapNotNull { line ->
        val matchResult = regex.find(line.trim())
        matchResult?.let {
            val (className, fileName, lineNumber) = it.destructured
            StackTraceElement(className, "", fileName, lineNumber.toInt())
        }
    }.toTypedArray()

    throwable.stackTrace = stackTraceElements

    return throwable
}

internal fun Crash.toEntity(): CrashEntity {
    return when (this) {
        is Crash.NativeCodeCrash -> toEntity()
        is Crash.UncaughtExceptionCrash -> toEntity()
    }
}

private fun Throwable.serialize(): ByteArray {
    val byteArrayOutputStream = ByteArrayOutputStream()
    ObjectOutputStream(byteArrayOutputStream).use { oos ->
        oos.writeObject(this)
    }
    return byteArrayOutputStream.toByteArray()
}

private fun ByteArray.deserializeThrowable(): Throwable {
    val byteArrayInputStream = ByteArrayInputStream(this)
    val throwable = ObjectInputStream(byteArrayInputStream).use { ois ->
        ois.readObject()
    }
    return throwable as Throwable
}

private fun CrashEntity.deserializeThrowable(): Throwable {
    return throwableData?.let {
        Result.runCatching { it.deserializeThrowable() }
            .getOrElse { error ->
                val message = error.message?.let { "Unable to restore: $it" } ?: "Caught error has missing message"
                restoreArchiveError(message)
            }
    } ?: restoreArchiveError("Missing ThrowableData")
}

private fun Crash.NativeCodeCrash.toEntity(): CrashEntity =
    CrashEntity(
        crashType = CrashType.NATIVE,
        uuid = uuid,
        runtimeTags = runtimeTags,
        breadcrumbs = breadcrumbs.map { Json.encodeToString(Breadcrumb.serializer(), it.toBreadcrumb()) },
        createdAt = timestamp,
        throwableData = null,
        stacktrace = "<native crash>",
        minidumpPath = minidumpPath,
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
        throwableData = throwable.serialize(),
        stacktrace = throwable.getStacktraceAsString(),
        minidumpPath = null,
        processType = null,
        extrasPath = null,
        remoteType = null,
    )

internal enum class CrashType {
    NATIVE, UNCAUGHT
}
