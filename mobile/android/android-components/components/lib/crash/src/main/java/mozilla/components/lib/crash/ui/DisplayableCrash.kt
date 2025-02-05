/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.ui

import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.db.CrashWithReports
import mozilla.components.lib.crash.db.ReportEntity

/**
 * Data class representing a Crash for the UI layer.
 * @property uuid the id of the crash.
 * @property stacktrace The stacktrace of the crash (if this crash was caused by an exception/throwable):
 * otherwise a string describing the type of crash.
 * @property createdAt Timestamp (in milliseconds) of when the crash happened.
 * @property reports The reports of the crash on our crash reporting services.
 */
internal data class DisplayableCrash(
    val uuid: String,
    val stacktrace: String,
    val createdAt: Long,
    val reports: List<Report>,
) {

    /**
     * Data class representing a crash report on our crash reporting services.
     * @property serviceName The Service name.
     * @property url The URL of this crash on this service.
     */
    data class Report(
        val serviceName: String,
        val url: String?,
    )

    override fun toString() = StringBuilder().apply {
        append(uuid)
        appendLine()
        append(stacktrace.lines().first())
        appendLine()

        reports.forEach { report ->
            append(" * ${report.serviceName}: ${report.url ?: "<No URL>"}")
            appendLine()
        }

        append("----")
        appendLine()
        append(stacktrace)
        appendLine()
    }.toString()
}

internal fun CrashWithReports.toCrash(reporter: CrashReporter) = DisplayableCrash(
    crash.uuid,
    crash.stacktrace,
    crash.createdAt,
    reports.map { it.toReport(reporter) },
)

internal fun ReportEntity.toReport(reporter: CrashReporter): DisplayableCrash.Report {
    val service = reporter.getCrashReporterServiceById(serviceId)
    val name = service?.name ?: serviceId
    val url = service?.createCrashReportUrl(reportId)
    return DisplayableCrash.Report(name, url)
}
