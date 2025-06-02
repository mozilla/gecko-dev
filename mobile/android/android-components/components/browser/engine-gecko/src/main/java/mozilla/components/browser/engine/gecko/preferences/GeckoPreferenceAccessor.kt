/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.preferences

import org.mozilla.geckoview.GeckoPreferenceController
import org.mozilla.geckoview.GeckoPreferenceController.GeckoPreference
import org.mozilla.geckoview.GeckoResult

/**
 * An interface for accessing Gecko preferences.
 *
 * This interface provides methods for getting and setting Gecko preferences of various types.
 *
 * It is important to note that all methods in this interface can potentially block the calling thread,
 * so they should not be called on the main thread.
 *
 * All methods return a [GeckoResult] object, which can be used to check if the operation was successful.
 */
interface GeckoPreferenceAccessor {
    /**
     * Gets the value of a Gecko preference.
     *
     * @param pref The name of the preference to get.
     * @return A [GeckoResult] object containing the value of the preference, or null if the preference does not exist.
     */
    fun getGeckoPref(pref: String): GeckoResult<GeckoPreference<*>?>

    /**
     * Sets the value of a Gecko preference.
     *
     * @param pref The name of the preference to set.
     * @param value The new value for the preference.
     * @param branch The preference branch to operate on.
     * @return A [GeckoResult] object indicating whether the operation was successful.
     */
    fun setGeckoPref(pref: String, value: String, branch: Int): GeckoResult<Void>

    /**
     * Sets an integer Gecko preference.
     *
     * @param pref The name of the preference to set.
     * @param value The integer value to set the preference to.
     * @param branch The preference branch to operate on.
     * @return A [GeckoResult] object indicating whether the operation was successful.
     */
    fun setGeckoPref(pref: String, value: Int, branch: Int): GeckoResult<Void>

    /**
     * Sets the value of a boolean Gecko preference.
     *
     * @param pref The name of the preference to set.
     * @param value The value to set the preference to.
     * @param branch The preference branch to operate on.
     * @return A [GeckoResult] object that can be used to check if the operation was successful.
     */
    fun setGeckoPref(pref: String, value: Boolean, branch: Int): GeckoResult<Void>

    /**
     * Clears a user branch Gecko preference.
     *
     * @param pref The name of the preference to clear.
     * @return A [GeckoResult] object indicating whether the operation was successful.
     */
    fun clearGeckoUserPref(pref: String): GeckoResult<Void>
}

internal class DefaultGeckoPreferenceAccessor : GeckoPreferenceAccessor {
    /**
     * Gets the value of the Gecko preference with the given name.
     *
     * @param pref The name of the preference to get.
     * @return A [GeckoResult] containing the [GeckoPreference] for the given preference, or null if the
     * preference does not exist. The [GeckoResult] will contain an error if the operation fails.
     */
    override fun getGeckoPref(pref: String): GeckoResult<GeckoPreference<*>?> {
        return GeckoPreferenceController.getGeckoPref(pref)
    }

    /**
     * Sets the value of a string Gecko preference.
     *
     * @param pref The name of the preference to set.
     * @param value The new value for the preference.
     * @param branch The preference branch to operate on.
     * @return A [GeckoResult] object indicating whether the operation was successful.
     */
    override fun setGeckoPref(
        pref: String,
        value: String,
        branch: Int,
    ): GeckoResult<Void> {
        return GeckoPreferenceController.setGeckoPref(pref, value, branch)
    }

    /**
     * Sets the value of an integer Gecko preference.
     *
     * @param pref The name of the preference to set.
     * @param value The integer value to set the preference to.
     * @param branch The preference branch to operate on.
     * @return A [GeckoResult] object that can be used to check if the operation was successful.
     */
    override fun setGeckoPref(
        pref: String,
        value: Int,
        branch: Int,
    ): GeckoResult<Void> {
        return GeckoPreferenceController.setGeckoPref(pref, value, branch)
    }

    /**
     * Sets the value of a boolean Gecko preference.
     *
     * @param pref The name of the preference to set.
     * @param value The boolean value to set the preference to.
     * @param branch The preference branch to operate on.
     * @return A [GeckoResult] object indicating whether the operation was successful.
     */
    override fun setGeckoPref(
        pref: String,
        value: Boolean,
        branch: Int,
    ): GeckoResult<Void> {
        return GeckoPreferenceController.setGeckoPref(pref, value, branch)
    }

    /**
     * Clears a Gecko user branch preference.
     *
     * @param pref The name of the preference to clear.
     * @return A [GeckoResult] object, which can be used to check if the operation was successful.
     */
    override fun clearGeckoUserPref(pref: String): GeckoResult<Void> {
        return GeckoPreferenceController.clearGeckoUserPref(pref)
    }
}
