/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.browsingmode

import android.content.Intent
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.utils.toSafeIntent
import org.mozilla.fenix.HomeActivity.Companion.PRIVATE_BROWSING_MODE
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppState
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

    /**
     * Updates the [BrowsingMode] based on the [Intent] that started the activity.
     *
     * @param intent The [Intent] that started the activity.
     */
    fun updateMode(intent: Intent? = null)
}

/**
 * Default implementation of [BrowsingModeManager] that tracks the current [BrowsingMode],
 * persists it to [Settings], and synchronizes it with [AppStore].
 *
 * @param intent The [Intent] that started the activity.
 * @param store The [BrowserStore] to observe the private tabs state from.
 * @param settings Used to persist the last known mode across sessions.
 * @param modeDidChange Callback that is invoked whenever the browsing mode changes.
 * @param updateAppStateMode Callback used to update the [AppState.mode].
 */
class DefaultBrowsingModeManager(
    intent: Intent?,
    private val store: BrowserStore,
    private val settings: Settings,
    private val modeDidChange: (BrowsingMode) -> Unit,
    private val updateAppStateMode: (BrowsingMode) -> Unit,
) : BrowsingModeManager {
    override var mode: BrowsingMode = getModeFromIntentOrLastKnown(intent)
        set(value) {
            field = value
            modeDidChange(value)
            settings.lastKnownMode = value
            updateAppStateMode(value)
        }

    override fun updateMode(intent: Intent?) {
        val mode = getModeFromIntentOrLastKnown(intent)
        if (this.mode != mode) {
            this.mode = mode
        }
    }

    /**
     * Returns the [BrowsingMode] set by the [intent] or the last known browsing mode based on
     * whether or not the user was in private mode and has any private tabs, otherwise fallback to
     * [BrowsingMode.Normal].
     */
    private fun getModeFromIntentOrLastKnown(intent: Intent?): BrowsingMode {
        intent?.toSafeIntent()?.let {
            if (it.hasExtra(PRIVATE_BROWSING_MODE)) {
                val startPrivateMode = it.getBooleanExtra(PRIVATE_BROWSING_MODE, false)
                return BrowsingMode.fromBoolean(isPrivate = startPrivateMode)
            }
        }

        if (settings.lastKnownMode.isPrivate && store.state.getNormalOrPrivateTabs(private = true).isNotEmpty()) {
            return BrowsingMode.Private
        }

        return BrowsingMode.Normal
    }
}
