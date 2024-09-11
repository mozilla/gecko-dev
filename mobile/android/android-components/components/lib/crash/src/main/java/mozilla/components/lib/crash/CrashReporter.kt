/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.app.ActivityOptions
import android.app.ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.annotation.StyleRes
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.lib.crash.db.CrashDatabase
import mozilla.components.lib.crash.db.insertCrashSafely
import mozilla.components.lib.crash.db.insertReportSafely
import mozilla.components.lib.crash.db.toCrash
import mozilla.components.lib.crash.db.toEntity
import mozilla.components.lib.crash.db.toReportEntity
import mozilla.components.lib.crash.handler.ExceptionHandler
import mozilla.components.lib.crash.notification.CrashNotification
import mozilla.components.lib.crash.prompt.CrashPrompt
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.crash.service.CrashTelemetryService
import mozilla.components.lib.crash.service.SendCrashReportService
import mozilla.components.lib.crash.service.SendCrashTelemetryService
import mozilla.components.support.base.android.NotificationsDelegate
import mozilla.components.support.base.log.logger.Logger

/**
 * Stores a list of `Breadcrumb` objects for the crash reporter.
 *
 * This is shared between multiple threads and needs to be thread-safe.
 */
private class BreadcrumbList(val maxBreadCrumbs: Int) {
    private val breadcrumbs = ArrayDeque<Breadcrumb>()

    @Synchronized
    internal fun copy(): ArrayList<Breadcrumb> {
        return ArrayList<Breadcrumb>(breadcrumbs)
    }

    @Synchronized
    internal fun add(breadcrumb: Breadcrumb) {
        if (breadcrumbs.size >= maxBreadCrumbs) {
            breadcrumbs.removeFirst()
        }
        breadcrumbs.add(breadcrumb)
    }
}

/**
 *
 * A generic crash reporter that can report crashes to multiple services.
 *
 * In the `onCreate()` method of your Application class create a `CrashReporter` instance and call `install()`:
 *
 * ```Kotlin
 *   CrashReporter(
 *     services = listOf(
 *       // List the crash reporting services you want to use
 *     )
 *   ).install(this)
 * ```
 *
 * With this minimal setup the crash reporting library will capture "uncaught exception" crashes and "native code"
 * crashes and forward them to the configured crash reporting services.
 *
 * @property enabled Enable/Disable crash reporting.
 *
 * @param services List of crash reporting services that should receive crash reports.
 * @param telemetryServices List of telemetry crash reporting services that should receive crash reports.
 * @param shouldPrompt Whether or not the user should be prompted to confirm sending crash reports.
 * @param enabled Enable/Disable crash reporting.
 * @param promptConfiguration Configuration for customizing the crash reporter prompt.
 * @param nonFatalCrashIntent A [PendingIntent] that will be launched if a non fatal crash (main process not affected)
 *                            happened. This gives the app the opportunity to show an in-app confirmation UI before
 *                            sending a crash report. See component README for details.
 * @param useLegacyReporting Enable/Disable handling crash reporting through a notification or system dialog.
 */
class CrashReporter internal constructor(
    private val services: List<CrashReporterService> = emptyList(),
    private val telemetryServices: List<CrashTelemetryService> = emptyList(),
    private val shouldPrompt: Prompt = Prompt.NEVER,
    enabled: Boolean = true,
    internal val promptConfiguration: PromptConfiguration = PromptConfiguration(),
    private val nonFatalCrashIntent: PendingIntent? = null,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    private val maxBreadCrumbs: Int = 30,
    private val notificationsDelegate: NotificationsDelegate,
    private val runtimeTagProviders: List<RuntimeTagProvider> = emptyList(),
    databaseProvider: () -> CrashDatabase,
    private val useLegacyReporting: Boolean = true,
) : CrashReporting {

    constructor(
        context: Context,
        services: List<CrashReporterService> = emptyList(),
        telemetryServices: List<CrashTelemetryService> = emptyList(),
        shouldPrompt: Prompt = Prompt.NEVER,
        enabled: Boolean = true,
        promptConfiguration: PromptConfiguration = PromptConfiguration(),
        nonFatalCrashIntent: PendingIntent? = null,
        scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
        maxBreadCrumbs: Int = 30,
        notificationsDelegate: NotificationsDelegate,
        runtimeTagProviders: List<RuntimeTagProvider> = emptyList(),
        useLegacyReporting: Boolean = true,
    ) : this(
        services = services,
        telemetryServices = telemetryServices,
        shouldPrompt = shouldPrompt,
        enabled = enabled,
        promptConfiguration = promptConfiguration,
        nonFatalCrashIntent = nonFatalCrashIntent,
        scope = scope,
        maxBreadCrumbs = maxBreadCrumbs,
        notificationsDelegate = notificationsDelegate,
        runtimeTagProviders = runtimeTagProviders,
        databaseProvider = { CrashDatabase.get(context) },
        useLegacyReporting = useLegacyReporting,
    )

    var enabled: Boolean = enabled

    private val database: CrashDatabase by lazy { databaseProvider() }

    internal val logger = Logger("mozac/CrashReporter")

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    private val crashBreadcrumbs = BreadcrumbList(maxBreadCrumbs)

    private val runtimeTags: Map<String, String>
        get() = runtimeTagProviders.fold(emptyMap()) { acc, provider -> acc + provider() }

    init {
        if (services.isEmpty() and telemetryServices.isEmpty()) {
            throw IllegalArgumentException("No crash reporter services defined")
        }
    }

    /**
     * Install this [CrashReporter] instance. At this point the component will be setup to collect crash reports.
     */
    fun install(applicationContext: Context): CrashReporter {
        instance = this

        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        val handler = ExceptionHandler(
            context = applicationContext,
            crashReporter = this,
            defaultExceptionHandler = defaultHandler,
        )
        Thread.setDefaultUncaughtExceptionHandler(handler)

        return this
    }

    /**
     * Checks to see if there are any unsent crash reports
     */
    suspend fun hasUnsentCrashReports(): Boolean {
        return database.crashDao().numberOfUnsentCrashes() > 0
    }

    /**
     * Fetches unsent crash reports from the crash reporter.
     */
    suspend fun unsentCrashReports(): List<Crash> {
        return database.crashDao().getCrashesWithoutReports()
            .map { it.toCrash() }
    }

    /**
     * Get a copy of the crashBreadcrumbs
     */
    fun crashBreadcrumbsCopy(): ArrayList<Breadcrumb> {
        return crashBreadcrumbs.copy()
    }

    /**
     * Submit a crash report to all registered services.
     */
    fun submitReport(crash: Crash, then: () -> Unit = {}): Job {
        return scope.launch {
            services.forEach { service ->
                val reportId = when (crash) {
                    is Crash.NativeCodeCrash -> service.report(crash)
                    is Crash.UncaughtExceptionCrash -> service.report(crash)
                }

                if (reportId != null) {
                    database.crashDao().insertReportSafely(service.toReportEntity(crash, reportId))
                }

                val reportUrl = reportId?.let { service.createCrashReportUrl(it) }

                logger.info("Submitted crash to ${service.name} (id=$reportId, url=$reportUrl)")
            }

            logger.info("Crash report submitted to ${services.size} services")
            withContext(Dispatchers.Main) {
                then()
            }
        }
    }

    /**
     * Submit a crash report to all registered telemetry services.
     */
    fun submitCrashTelemetry(crash: Crash, then: () -> Unit = {}): Job {
        return scope.launch {
            telemetryServices.forEach { telemetryService ->
                when (crash) {
                    is Crash.NativeCodeCrash -> telemetryService.record(crash)
                    is Crash.UncaughtExceptionCrash -> telemetryService.record(crash)
                }
            }

            logger.info("Crash report submitted to ${telemetryServices.size} telemetry services")
            withContext(Dispatchers.Main) {
                then()
            }
        }
    }

    /**
     * Submit a caught exception report to all registered services.
     */
    override fun submitCaughtException(throwable: Throwable): Job {
        /*
         * if stacktrace is empty, replace throwable with UnexpectedlyMissingStacktrace exception so
         * we can figure out which module is submitting caught exception reports without a stacktrace.
         */
        var reportThrowable = throwable
        if (throwable.stackTrace.isEmpty()) {
            reportThrowable = CrashReporterException.UnexpectedlyMissingStacktrace("Missing Stacktrace", throwable)
        }

        logger.info("Caught Exception report submitted to ${services.size} services")
        return scope.launch {
            services.forEach {
                it.report(reportThrowable, crashBreadcrumbsCopy())
            }
        }
    }

    /**
     * Add a crash breadcrumb to all registered services with breadcrumb support.
     *
     * ```Kotlin
     *   crashReporter.recordCrashBreadcrumb(
     *       Breadcrumb("Settings button clicked", data, "UI", Level.INFO, Type.USER)
     *   )
     * ```
     */
    override fun recordCrashBreadcrumb(breadcrumb: Breadcrumb) {
        crashBreadcrumbs.add(breadcrumb)
    }

    /**
     * Called when a crash occurs. The crash information will be persisted locally along with
     * relevant runtime tags, and this function will decide whether to send a report automatically,
     * prompt the user to send a report, or do nothing.
     */
    internal fun onCrash(context: Context, crash: Crash) {
        if (!enabled) {
            return
        }

        val crashWithTags = crash.withTags(runtimeTags)

        logger.info("Received crash: $crashWithTags")

        database.crashDao().insertCrashSafely(crashWithTags.toEntity())

        if (telemetryServices.isNotEmpty()) {
            sendCrashTelemetry(context, crashWithTags)
        }

        // If crash is native code and non fatal then the view will handle the user prompt
        if (shouldSendIntent(crashWithTags)) {
            // App has registered a pending intent
            sendNonFatalCrashIntent(context, crashWithTags)
            return
        }

        // If an application chooses to handle crash reporting themselves they will want to opt-out
        // of showing a prompt or notification.
        if (!useLegacyReporting) {
            return
        }

        if (services.isNotEmpty()) {
            if (CrashPrompt.shouldPromptForCrash(shouldPrompt, crashWithTags)) {
                showPromptOrNotification(context, crashWithTags)
            } else {
                sendCrashReport(context, crashWithTags)
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun sendNonFatalCrashIntent(context: Context, crash: Crash) {
        logger.info("Invoking non-fatal PendingIntent")

        val additionalIntent = Intent()
        crash.fillIn(additionalIntent)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            val onFinished = null
            val handler = null
            val requiredPermission = null
            val activityOptions = ActivityOptions.makeBasic()
            activityOptions.pendingIntentBackgroundActivityStartMode =
                MODE_BACKGROUND_ACTIVITY_START_ALLOWED

            nonFatalCrashIntent?.send(
                context,
                0,
                additionalIntent,
                onFinished,
                handler,
                requiredPermission,
                activityOptions.toBundle(),
            )
        } else {
            nonFatalCrashIntent?.send(context, 0, additionalIntent)
        }
    }

    internal fun showPromptOrNotification(context: Context, crash: Crash) {
        if (services.isEmpty()) {
            return
        }

        if (CrashNotification.shouldShowNotificationInsteadOfPrompt(crash)) {
            // If this is a fatal crash taking down the app then we may not be able to show a crash reporter
            // prompt on Android Q+. Unfortunately it's not possible to easily determine if we can launch an
            // activity here. So instead we fallback to just showing a notification
            // https://developer.android.com/preview/privacy/background-activity-starts
            logger.info("Showing notification")
            showNotification(context, crash)
        } else {
            logger.info("Showing prompt")
            showPrompt(context, crash)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun showNotification(context: Context, crash: Crash) {
        val notification = CrashNotification(context, crash, promptConfiguration, notificationsDelegate)
        notification.show()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun sendCrashReport(context: Context, crash: Crash) {
        ContextCompat.startForegroundService(context, SendCrashReportService.createReportIntent(context, crash))
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun sendCrashTelemetry(context: Context, crash: Crash) {
        ContextCompat.startForegroundService(context, SendCrashTelemetryService.createReportIntent(context, crash))
    }

    @VisibleForTesting
    internal fun showPrompt(context: Context, crash: Crash) {
        val prompt = CrashPrompt(context, crash)
        prompt.show()
    }

    private fun shouldSendIntent(crash: Crash): Boolean {
        return if (nonFatalCrashIntent == null) {
            // If the app has not registered any intent then we can't send one.
            false
        } else {
            // If this is a native code crash in a foreground child process then we can recover
            // and can notify the app. Background child process crashes will be recovered from
            // automatically, and main process crashes cannot be recovered from, so we do not
            // send the intent for those.
            crash is Crash.NativeCodeCrash && crash.processType == Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD
        }
    }

    internal fun getCrashReporterServiceById(id: String): CrashReporterService? {
        return services.firstOrNull { it.id == id }
    }

    enum class Prompt {
        /**
         * Never prompt the user. Always submit crash reports immediately.
         */
        NEVER,

        /**
         * Only prompt the user for native code crashes.
         */
        ONLY_NATIVE_CRASH,

        /**
         * Always prompt the user for confirmation before sending crash reports.
         */
        ALWAYS,
    }

    /**
     * Configuration for the crash reporter prompt.
     */
    data class PromptConfiguration(
        internal val appName: String = "App",
        internal val organizationName: String = "Mozilla",
        internal val message: String? = null,
        @StyleRes internal val theme: Int = R.style.Theme_Mozac_CrashReporter,
    )

    companion object {
        @Volatile
        private var instance: CrashReporter? = null

        @VisibleForTesting
        internal fun reset() {
            instance = null
        }

        internal val requireInstance: CrashReporter
            get() = instance ?: throw IllegalStateException(
                "You need to call install() on your CrashReporter instance from Application.onCreate().",
            )
    }
}

/**
 * A base class for exceptions describing crash reporter exception.
 */
internal abstract class CrashReporterException(message: String, cause: Throwable?) : Exception(message, cause) {
    /**
     * Stacktrace was expected to be present, but it wasn't.
     */
    internal class UnexpectedlyMissingStacktrace(
        message: String,
        cause: Throwable?,
    ) : CrashReporterException(message, cause)
}
