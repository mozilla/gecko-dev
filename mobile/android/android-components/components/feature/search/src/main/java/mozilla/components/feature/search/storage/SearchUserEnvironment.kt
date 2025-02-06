/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

/**
 * The user's environment that is used for filtering the search configuration.
 *
 * @property locale the current locale of the application that the user is using.
 * @property region the home region that the user is currently identified as being within.
 * @property update_channel the update channel of the user's build.
 * @property distribution_id the distribution id for the user's build.
 * @property experiment the search related experiment id that the user is included within.
 * @property app_name the application name that the user is using.
 * @property version the application version that the user is using.
 */
@Suppress("ConstructorParameterNaming")
data class SearchUserEnvironment(
    val locale: String,
    val region: String,
    val update_channel: SearchUpdateChannel,
    val distribution_id: String,
    val experiment: String,
    val app_name: SearchApplicationName,
    val version: String,
)

/**
 * The list of possible update channels for a user's build.
 * Use `default` for a self-build or an unknown channel.
*/
enum class SearchUpdateChannel {
    Default,
    Nightly,
    Beta,
    Release,
}

/**
 * The list of possible application names.
 */
enum class SearchApplicationName(val value: String) {
    FirefoxAndroid("Firefox Fenix"),
    FocusAndroid("Firefox Focus"), ;

    /**
     * Contains the methods to get enum value.
     */
    companion object {
        /**
         * Get [SearchApplicationName] enum from alias app name.
         *
         * @param value [String] value of the [SearchApplicationName]
         */
        fun fromAlias(value: String): SearchApplicationName? =
            entries.firstOrNull { it.value == value }
    }
}
