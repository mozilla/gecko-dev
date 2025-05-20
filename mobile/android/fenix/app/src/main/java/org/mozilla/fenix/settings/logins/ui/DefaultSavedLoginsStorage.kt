/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import org.mozilla.fenix.utils.Settings

/**
 * An interface to persist the state of the saved logins screen.
 */
interface SavedLoginsStorage {
    /**
     * Indicates the sort order of the saved logins list.
     */
    var savedLoginsSortOrder: LoginsSortOrder
}

/**
 * A default implementation of `SavedLoginsStorage`.
 *
 * @property settings The settings object used to persist the saved logins screen state.
 */
class DefaultSavedLoginsStorage(
    val settings: Settings,
) : SavedLoginsStorage {
    override var savedLoginsSortOrder
        get() = LoginsSortOrder.fromString(settings.loginsListSortOrder)
        set(value) {
            settings.loginsListSortOrder = value.asString
        }
}
