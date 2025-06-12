/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.browsingmode

import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.utils.Settings

/**
 * Enum that represents whether or not private browsing is active.
 */
enum class BrowsingMode {
    Normal, Private;

    /**
     * Returns true if the [BrowsingMode] is [Private]
     */
    val isPrivate get() = this == Private

    companion object {

        /**
         * Convert a boolean into a [BrowsingMode].
         * True corresponds to [Private] and false corresponds to [Normal].
         */
        fun fromBoolean(isPrivate: Boolean) = if (isPrivate) Private else Normal
    }
}

interface BrowsingModeManager {
    var mode: BrowsingMode
}

/**
 * Default implementation of [BrowsingModeManager] that tracks the current [BrowsingMode],
 * persists it to [Settings], and synchronizes it with [AppStore].
 *
 * @param initialMode The initial [BrowsingMode]
 * @param settings Used to persist the last known mode across sessions.
 * @param appStore The [AppStore] that receives dispatched [AppAction.ModeChange] events.
 * @param modeDidChange Callback that is invoked whenever the browsing mode changes.
 */
class DefaultBrowsingModeManager(
    private var initialMode: BrowsingMode,
    private val settings: Settings,
    private val appStore: AppStore,
    private val modeDidChange: (BrowsingMode) -> Unit,
) : BrowsingModeManager {
    override var mode: BrowsingMode = initialMode
        set(value) {
            field = value
            modeDidChange(value)
            settings.lastKnownMode = value
            appStore.dispatch(AppAction.BrowsingModeManagerModeChanged(mode = value))
        }
}
