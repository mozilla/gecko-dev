/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.fission

/**
 * The web content isolation strategy to use with fission.
 */
enum class WebContentIsolationStrategy(val strategy: Int) {
    /**
     * All web content is loaded into a shared `web` content process. This is similar to the
     * non-Fission behaviour, however remote subframes may still be used for sites with special
     * isolation behaviour, such as extension or mozillaweb content processes.
     */
    ISOLATE_NOTHING(0),

    /**
     * Web content is always isolated into its own `webIsolated` content process based on site-origin,
     * and will only load in a shared `web` content process if site-origin could not be determined.
     */
    ISOLATE_EVERYTHING(1),

    /**
     * Only isolates web content loaded by sites which are considered "high value". A site is
     * considered "high value" if it has been granted a `highValue*` permission by the permission
     * manager, which is done in response to certain actions.
     */
    ISOLATE_HIGH_VALUE(2),
    ;

    /**
     * Companion object [WebContentIsolationStrategy].
     */
    companion object {
        /**
         * Convenience method to map an integer to a [WebContentIsolationStrategy].
         *
         * @param strategy The specified strategy as an int.
         * @return A [WebContentIsolationStrategy] or [ISOLATE_HIGH_VALUE], for an unknown int.
         */
        fun fromValue(strategy: Int): WebContentIsolationStrategy = when (strategy) {
            0 -> ISOLATE_NOTHING
            1 -> ISOLATE_EVERYTHING
            2 -> ISOLATE_HIGH_VALUE
            else ->
                ISOLATE_HIGH_VALUE
        }
    }
}
