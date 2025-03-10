/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.app.Application
import androidx.annotation.VisibleForTesting
import com.adjust.sdk.Adjust
import com.adjust.sdk.AdjustConfig
import com.adjust.sdk.AdjustEvent
import com.adjust.sdk.Constants.ADJUST_PREINSTALL_SYSTEM_PROPERTY_PATH
import com.adjust.sdk.LogLevel
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.AdjustAttribution
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class AdjustMetricsService(
    private val application: Application,
    private val storage: MetricsStorage,
    private val crashReporter: CrashReporter,
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO,
) : MetricsService {
    override val type = MetricServiceType.Marketing
    private val logger = Logger("AdjustMetricsService")

    override fun start() {
        val settings = application.components.settings
        if ((BuildConfig.ADJUST_TOKEN.isNullOrBlank())) {
            logger.info("No adjust token defined")

            if (Config.channel.isReleased) {
                throw IllegalStateException("No adjust token defined for release build")
            }

            return
        }

        if (alreadyKnown(settings)) {
            logger.info("Attribution already retrieved")
            return
        }

        System.setProperty(ADJUST_PREINSTALL_SYSTEM_PROPERTY_PATH, "/preload/etc/adjust.preinstall")

        val config = AdjustConfig(
            application,
            BuildConfig.ADJUST_TOKEN,
            AdjustConfig.ENVIRONMENT_PRODUCTION,
            true,
        )
        config.enablePreinstallTracking()

        val timerId = AdjustAttribution.adjustAttributionTime.start()
        config.setOnAttributionChangedListener {
            AdjustAttribution.adjustAttributionTime.stopAndAccumulate(timerId)

            if (!it.network.isNullOrEmpty()) {
                settings.adjustNetwork = it.network
                AdjustAttribution.network.set(it.network)
            }
            if (!it.adgroup.isNullOrEmpty()) {
                settings.adjustAdGroup = it.adgroup
                AdjustAttribution.adgroup.set(it.adgroup)
            }
            if (!it.creative.isNullOrEmpty()) {
                settings.adjustCreative = it.creative
                AdjustAttribution.creative.set(it.creative)
            }
            if (!it.campaign.isNullOrEmpty()) {
                settings.adjustCampaignId = it.campaign
                AdjustAttribution.campaign.set(it.campaign)
            }

            triggerPing()
        }

        config.setLogLevel(LogLevel.SUPPRESS)
        Adjust.initSdk(config)
        Adjust.enable()
    }

    override fun stop() {
        Adjust.disable()
        Adjust.gdprForgetMe(application.applicationContext)
    }

    @Suppress("TooGenericExceptionCaught")
    override fun track(event: Event) {
        CoroutineScope(dispatcher).launch {
            try {
                if (event is Event.GrowthData) {
                    if (storage.shouldTrack(event)) {
                        Adjust.trackEvent(AdjustEvent(event.tokenName))
                        storage.updateSentState(event)
                    } else {
                        storage.updatePersistentState(event)
                    }
                }
            } catch (e: Exception) {
                crashReporter.submitCaughtException(e)
            }
        }
    }

    override fun shouldTrack(event: Event): Boolean =
        event is Event.GrowthData

    companion object {
        @VisibleForTesting
        internal fun alreadyKnown(settings: Settings): Boolean {
            return settings.adjustCampaignId.isNotEmpty() || settings.adjustNetwork.isNotEmpty() ||
                settings.adjustCreative.isNotEmpty() || settings.adjustAdGroup.isNotEmpty()
        }

        private fun triggerPing() {
            CoroutineScope(Dispatchers.IO).launch {
                Pings.adjustAttribution.submit()
            }
        }
    }
}
