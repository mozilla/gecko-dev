/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import org.mozilla.fenix.utils.Settings

/**
 * Interface for retrieving the distribution ID associated with the current installation.
 */
interface DistributionSettings {
    /**
     * Returns the distribution ID used to identify the app's distribution source (e.g., preinstall partner).
     *
     * @return A non-null string representing the distribution ID. May be blank if not set.
     */
    fun getDistributionId(): String

    /**
     * Persists the provided distribution ID for future retrieval.
     *
     * @param id A non-null string representing the distribution ID to store.
     */
    fun saveDistributionId(id: String)
}

/**
 * Default implementation of [DistributionSettings] that retrieves the distribution ID
 * from the provided [Settings] instance.
 *
 * @param settings The [Settings] object used to persist and retrieve the distribution ID.
 */
class DefaultDistributionSettings(
    private val settings: Settings,
) : DistributionSettings {
    /**
     * Returns the stored distribution ID from [settings].
     *
     * @return The persisted distribution ID as a string.
     */
    override fun getDistributionId() = settings.distributionId

    /**
     * Stores the given distribution ID in [settings].
     *
     * @param id A non-null string representing the distribution ID to persist.
     */
    override fun saveDistributionId(id: String) {
        settings.distributionId = id
    }
}
