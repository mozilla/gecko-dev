/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import androidx.preference.PreferenceManager
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.RunWhenReadyQueue
import mozilla.telemetry.glean.Glean
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.Usage
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import java.util.UUID

private class EventWrapper<T : Enum<T>>(
    private val recorder: ((Map<T, String>?) -> Unit),
    private val keyMapper: ((String) -> T)? = null,
) {

    /**
     * Converts snake_case string to camelCase.
     */
    private fun String.asCamelCase(): String {
        val parts = split("_")
        val builder = StringBuilder()

        for ((index, part) in parts.withIndex()) {
            if (index == 0) {
                builder.append(part)
            } else {
                builder.append(part[0].uppercase())
                builder.append(part.substring(1))
            }
        }

        return builder.toString()
    }

    fun track(event: Event) {
        val extras = if (keyMapper != null) {
            event.extras?.mapKeys { (key) ->
                keyMapper.invoke(key.toString().asCamelCase())
            }
        } else {
            null
        }

        @Suppress("DEPRECATION")
        // FIXME(#19967): Migrate to non-deprecated API.
        this.recorder(extras)
    }
}

@Suppress("DEPRECATION")
// FIXME(#19967): Migrate to non-deprecated API.
private val Event.wrapper: EventWrapper<*>?
    get() = null

/**
 * Service responsible for sending the activation and installation pings.
 */
class GleanMetricsService(
    private val context: Context,
    private val runWhenReadyQueue: RunWhenReadyQueue = context.components.performance.visualCompletenessQueue.queue,
    private val gleanProfileId: GleanProfileId = UsageProfileId(),
    private val gleanProfileIdStore: GleanProfileIdStore = GleanProfileIdPreferenceStore(context),
) : MetricsService {
    override val type = MetricServiceType.Data

    private val logger = Logger("GleanMetricsService")
    private var initialized = false

    private val activationPing = ActivationPing(context)

    override fun start() {
        logger.debug("Enabling Glean.")
        // Initialization of Glean already happened in FenixApplication.
        Glean.setCollectionEnabled(true)

        if (initialized) return
        initialized = true

        checkAndSetUsageProfileId()

        // The code below doesn't need to execute immediately, so we'll add them to the visual
        // completeness task queue to be run later.
        runWhenReadyQueue.runIfReadyOrQueue {
            // We have to initialize Glean *on* the main thread, because it registers lifecycle
            // observers. However, the activation ping must be sent *off* of the main thread,
            // because it calls Google ad APIs that must be called *off* of the main thread.
            // These two things actually happen in parallel, but that should be ok because Glean
            // can handle events being recorded before it's initialized.
            Glean.registerPings(Pings)

            activationPing.checkAndSend()
        }
    }

    override fun stop() {
        Glean.setCollectionEnabled(false)
        unsetUsageProfileId()
    }

    override fun track(event: Event) {
        event.wrapper?.track(event)
    }

    override fun shouldTrack(event: Event): Boolean {
        return event.wrapper != null
    }

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

private class UsageProfileId : GleanProfileId {
    companion object {
        /**
         * Glean doesn't have an API to remove a value,
         * so we have to use this canary value.
         * (would also allow us to notice if we ever accidentally sent data after we shouldn't).
         */
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

private class GleanProfileIdPreferenceStore(context: Context) : GleanProfileIdStore {
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
