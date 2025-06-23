/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.service.nimbus.messaging.FxNimbusMessaging
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.BootUtils
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.ensureMarketingChannelExists
import org.mozilla.fenix.utils.IntentUtils
import org.mozilla.fenix.utils.createBaseNotification
import java.util.concurrent.CancellationException
import java.util.concurrent.TimeUnit

const val CLICKED_MESSAGE_ID = "clickedMessageId"
const val DISMISSED_MESSAGE_ID = "dismissedMessageId"

/**
 * Timeout duration (in milliseconds) for the fetch experiments operation, covering both the
 * network request and any required post-processing.
 *
 * This is a background task, not initiated by the user, so a slightly higher timeout is acceptable.
 *
 * If the operation doesn't complete within this time, it will be retried on the next app launch
 * or during the next scheduled sync interval.
 *
 * **Six seconds will cover 99% of users** based on Nimbus research.
 * @see `https://sql.telemetry.mozilla.org/queries/91863/source?p_days=30#227434`.
 */
private const val NIMBUS_FETCH_OPERATION_TIMEOUT_MILLIS: Long = 6000

private const val NIMBUS_APPLY_OPERATION_TIMEOUT_MILLIS: Long = 500

/**
 * The total timeout duration (in milliseconds) for the Nimbus fetch and apply operations.
 */
private const val NIMBUS_UPDATE_OPERATION_TIMEOUT_MILLIS =
    NIMBUS_FETCH_OPERATION_TIMEOUT_MILLIS + NIMBUS_APPLY_OPERATION_TIMEOUT_MILLIS

private val LOGGER = Logger("MessageNotificationWorker")

/**
 * Background [CoroutineWorker] that polls Nimbus for available [Message]s at a given interval.
 * A [Notification] will be created using the configuration of the next highest priority [Message]
 * if it has not already been displayed.
 */
class MessageNotificationWorker(
    context: Context,
    workerParameters: WorkerParameters,
) : CoroutineWorker(context, workerParameters) {

    @SuppressWarnings("ReturnCount")
    override suspend fun doWork(): Result {
        val context = applicationContext
        val nimbus = context.components.nimbus

        // Refresh messages from Nimbus to ensure getNextMessage reflects the latest available content.
        tryFetchAndApplyNimbusExperiments(nimbus.sdk).apply {
            if (this) {
                LOGGER.info("Successfully fetched and applied Nimbus experiments.")
            } else {
                LOGGER.info("Failed to fetch and apply Nimbus experiments.")
            }
        }

        val messaging = nimbus.messaging

        val nextMessage =
            messaging.getNextMessage(FenixMessageSurfaceId.NOTIFICATION)
                ?: return Result.success()

        val currentBootUniqueIdentifier = BootUtils.getBootIdentifier(context)
        //  Device has NOT been power cycled.
        if (nextMessage.hasShownThisCycle(currentBootUniqueIdentifier)) {
            return Result.success()
        }

        // Update message as displayed.
        messaging.onMessageDisplayed(nextMessage, currentBootUniqueIdentifier)

        context.components.notificationsDelegate.notify(
            MESSAGE_TAG,
            SharedIdsHelper.getIdForTag(context, nextMessage.id),
            buildNotification(
                context,
                nextMessage,
            ),
        )

        return Result.success()
    }

    private fun buildNotification(
        context: Context,
        message: Message,
    ): Notification {
        val onClickPendingIntent = createOnClickPendingIntent(context, message)
        val onDismissPendingIntent = createOnDismissPendingIntent(context, message)

        return createBaseNotification(
            context,
            ensureMarketingChannelExists(context),
            message.title,
            message.text,
            onClickPendingIntent,
            onDismissPendingIntent,
        )
    }

    private fun createOnClickPendingIntent(
        context: Context,
        message: Message,
    ): PendingIntent {
        val intent = Intent(context, NotificationClickedReceiverActivity::class.java)
        intent.putExtra(CLICKED_MESSAGE_ID, message.id)
        intent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS)

        // Activity intent.
        return PendingIntent.getActivity(
            context,
            SharedIdsHelper.getNextIdForTag(context, NOTIFICATION_PENDING_INTENT_TAG),
            intent,
            IntentUtils.defaultIntentPendingFlags,
        )
    }

    private fun createOnDismissPendingIntent(
        context: Context,
        message: Message,
    ): PendingIntent {
        val intent = Intent(context, NotificationDismissedService::class.java)
        intent.putExtra(DISMISSED_MESSAGE_ID, message.id)

        // Service intent.
        return PendingIntent.getService(
            context,
            SharedIdsHelper.getNextIdForTag(context, NOTIFICATION_PENDING_INTENT_TAG),
            intent,
            IntentUtils.defaultIntentPendingFlags,
        )
    }

    companion object {
        private const val NOTIFICATION_PENDING_INTENT_TAG = "org.mozilla.fenix.message"
        private const val MESSAGE_TAG = "org.mozilla.fenix.message.tag"
        private const val MESSAGE_WORK_NAME = "org.mozilla.fenix.message.work"

        /**
         * Initialize the [CoroutineWorker] to begin polling Nimbus.
         */
        fun setMessageNotificationWorker(context: Context) {
            val messaging = FxNimbusMessaging.features.messaging
            val featureConfig = messaging.value()
            val notificationConfig = featureConfig.notificationConfig
            val pollingInterval = notificationConfig.refreshInterval.toLong()

            val messageWorkRequest = PeriodicWorkRequestBuilder<MessageNotificationWorker>(
                pollingInterval,
                TimeUnit.MINUTES,
            ).build()

            val instanceWorkManager = WorkManager.getInstance(context)
            instanceWorkManager.enqueueUniquePeriodicWork(
                MESSAGE_WORK_NAME,
                // We want to keep any existing scheduled work, unless
                // when we're under test.
                if (messaging.isUnderTest()) {
                    ExistingPeriodicWorkPolicy.CANCEL_AND_REENQUEUE
                } else {
                    ExistingPeriodicWorkPolicy.KEEP
                },
                messageWorkRequest,
            )
        }

        /**
         * @return `true` if the fetch and apply operations were successfully completed within the
         * given [operationTimeout].
         */
        @VisibleForTesting
        internal suspend fun tryFetchAndApplyNimbusExperiments(
            nimbusSdk: NimbusApi,
            operationTimeout: Long = NIMBUS_UPDATE_OPERATION_TIMEOUT_MILLIS,
            experimentsFetched: CompletableDeferred<Unit> = CompletableDeferred(),
            experimentsApplied: CompletableDeferred<Unit> = CompletableDeferred(),
        ): Boolean {
            val nimbusExperimentsObserver = NimbusExperimentsObserver(
                nimbusSdk = nimbusSdk,
                experimentsFetched = experimentsFetched,
                experimentsApplied = experimentsApplied,
            )

            nimbusSdk.register(nimbusExperimentsObserver)

            return try {
                withTimeoutOrNull(operationTimeout) {
                    nimbusSdk.fetchExperiments()
                    LOGGER.debug("Fetching experiments.")
                    experimentsFetched.await()

                    LOGGER.debug("Applying pending experiments.")
                    experimentsApplied.await()
                }
                experimentsApplied.isCompleted
            } catch (e: CancellationException) {
                LOGGER.warn("Nimbus experiments operation timed out.")
                false
            } finally {
                nimbusSdk.unregister(nimbusExperimentsObserver)

                experimentsFetched.complete(Unit)
                experimentsApplied.complete(Unit)
            }
        }

        private class NimbusExperimentsObserver(
            val nimbusSdk: NimbusApi,
            val experimentsFetched: CompletableDeferred<Unit>,
            val experimentsApplied: CompletableDeferred<Unit>,
        ) : NimbusInterface.Observer {
            override fun onExperimentsFetched() {
                experimentsFetched.complete(Unit)
                LOGGER.debug("Experiments fetched.")
                nimbusSdk.applyPendingExperiments()
            }

            override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
                experimentsApplied.complete(Unit)
                LOGGER.debug("Pending experiments applied.")
            }
        }
    }
}

/**
 * When a [Message] [Notification] is dismissed by the user record telemetry data and update the
 * [Message.metadata].
 *
 * This Service is only intended to be used by the [MessageNotificationWorker.createOnDismissPendingIntent] function.
 */
class NotificationDismissedService : LifecycleService() {

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)

        if (intent != null) {
            val messaging = applicationContext.components.nimbus.messaging

            lifecycleScope.launch {
                // Get the relevant message.
                val message = intent.getStringExtra(DISMISSED_MESSAGE_ID)?.let { messageId ->
                    messaging.getMessage(messageId)
                }

                if (message != null) {
                    // Update message as 'dismissed'.
                    messaging.onMessageDismissed(message)
                }
            }
        }

        return START_REDELIVER_INTENT
    }
}

/**
 * When a [Message] [Notification] is clicked by the user record telemetry data and update the
 * [Message.metadata].
 *
 * This Activity is only intended to be used by the [MessageNotificationWorker.createOnClickPendingIntent] function.
 */
class NotificationClickedReceiverActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val messaging = applicationContext.components.nimbus.messaging

        lifecycleScope.launch {
            // Get the relevant message.
            val message = intent.getStringExtra(CLICKED_MESSAGE_ID)?.let { messageId ->
                messaging.getMessage(messageId)
            }

            if (message != null) {
                // Update message as 'clicked'.
                messaging.onMessageClicked(message)

                // Create the intent.
                val intent = messaging.getIntentForMessage(message)
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK)

                // Start the message intent.
                startActivity(intent)
            }
        }

        // End this activity.
        finish()
    }
}
