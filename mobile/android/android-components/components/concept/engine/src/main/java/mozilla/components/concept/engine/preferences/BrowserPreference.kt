/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.preferences

/**
 * The container for representing a browser preference.
 *
 * @property pref The name of the browser preference.
 * @property value The value of the preference that is currently active.
 * @property defaultValue The default value of the preference.
 * This is the browser recommended base or value to return to.
 * @property userValue The user value of the preference.
 * This is the value the user or a system set instead of the default value.
 * @property hasUserChangedValue Whether the user value of the preference has changed from the default.
 */
data class BrowserPreference<T>(
    val pref: String,
    val value: T? = null,
    val defaultValue: T? = null,
    val userValue: T? = null,
    val hasUserChangedValue: Boolean,
)
