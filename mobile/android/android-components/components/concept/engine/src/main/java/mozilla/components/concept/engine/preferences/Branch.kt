/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.preferences

/**
 * Represents the preference branch.
 */
enum class Branch {

    /**
     * The user branch is for preferences that do not change the default and are stored for the user.
     */
    USER,

    /**
     * The default branch will adjust the default for the preference. Default preferences are a sensible
     * choice for users or functionality to revert back to.
     *
     * Note: Changing a default value does not necessarily change the active value of the preference.
     */
    DEFAULT,
}
