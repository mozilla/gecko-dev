/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.preference.PreferenceManager
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.GleanMetrics.Usage
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import java.util.UUID

/**
 * Metrics service which encapsulates sending the usage reporting ping.
 * @param lifecycleOwner the top level container whose lifecycle is followed by usage data
 * @param gleanUsageReportingLifecycleObserver this can be provided to control
 * the start / stop sending events for the usage reporting ping
 * @param gleanUsageReporting this can be provided to encapsulate interactions with the glean API
 * @param gleanProfileId the glean profile id for usage reporting
 * @param gleanProfileIdStore the app level storage mechanism for the glean profile id
 */
class GleanUsageReportingMetricsService(
    private val lifecycleOwner: LifecycleOwner = ProcessLifecycleOwner.get(),
    private val gleanUsageReportingLifecycleObserver: LifecycleEventObserver = GleanUsageReportingLifecycleObserver(),
    private val gleanUsageReporting: GleanUsageReportingApi = GleanUsageReporting(),
    private val gleanProfileId: GleanProfileId = UsageProfileId(),
    private val gleanProfileIdStore: GleanProfileIdStore,
) : MetricsService {

    override val type: MetricServiceType = MetricServiceType.UsageReporting
    private val logger = Logger("GleanUsageReportingMetricsService")

    override fun start() {
        logger.debug("Start GleanUsageReportingMetricsService")
        gleanUsageReporting.setEnabled(true)
        checkAndSetUsageProfileId()
        lifecycleOwner.lifecycle.addObserver(gleanUsageReportingLifecycleObserver)
    }

    override fun stop() {
        logger.debug("Stop GleanUsageReportingMetricsService")
        gleanUsageReporting.setEnabled(false)
        lifecycleOwner.lifecycle.removeObserver(gleanUsageReportingLifecycleObserver)
        gleanUsageReporting.requestDataDeletion()
        unsetUsageProfileId()
    }

    override fun track(event: Event) {
        // no-op
    }

    override fun shouldTrack(event: Event): Boolean = false

    private fun checkAndSetUsageProfileId() {
        val profileId = gleanProfileIdStore.profileId
        if (profileId == null) {
            gleanProfileIdStore.profileId = gleanProfileId.generateAndSet().toString()
        } else {
            gleanProfileId.set(UUID.fromString(profileId))
        }
    }

    private fun unsetUsageProfileId() {
        gleanProfileId.unset()
        gleanProfileIdStore.clear()
    }
}

/**
 * An abstraction to represent a profile id as required by Glean.
 */
interface GleanProfileId {
    /**
     * Create a random UUID and set it in Glean.
     */
    fun generateAndSet(): UUID

    /**
     * Set the given profile id in Glean.
     */
    fun set(profileId: UUID)

    /**
     * Unset the current profile id in Glean.
     */
    fun unset()
}

/**
 * Represents the profile id used by Glean for usage reporting.
 */
class UsageProfileId : GleanProfileId {

    /**
     * Glean doesn't have an API to remove a value,
     * so we have to use this canary value.
     * (would also allow us to notice if we ever accidentally sent data after we shouldn't).
     */
    companion object {
        private const val CANARY_VALUE = "beefbeef-beef-beef-beef-beeefbeefbee"
    }

    override fun generateAndSet(): UUID = Usage.profileId.generateAndSet()
    override fun set(profileId: UUID) {
        Usage.profileId.set(profileId)
    }

    override fun unset() {
        Usage.profileId.set(UUID.fromString(CANARY_VALUE))
    }
}

/**
 * An abstraction to represent the storage mechanism within the app for a Glean profile id.
 */
interface GleanProfileIdStore {
    /**
     * Property allowing access to the profile id which is stored.
     */
    var profileId: String?

    /**
     * Remove the stored profile id.
     */
    fun clear()
}

/**
 * The actual preference store used to store the Glean profile id.
 */
class GleanProfileIdPreferenceStore(context: Context) : GleanProfileIdStore {
    private val defaultSharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
    private val preferenceKey = context.getPreferenceKey(R.string.pref_key_glean_usage_profile_id)

    override var profileId: String?
        get() = defaultSharedPreferences.getString(
            preferenceKey,
            null,
        )
        set(value) {
            defaultSharedPreferences.edit()
                .putString(
                    preferenceKey,
                    value,
                ).apply()
        }

    override fun clear() {
        defaultSharedPreferences.edit()
            .remove(preferenceKey).apply()
    }
}
