/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.store

import android.content.Context
import androidx.fragment.app.FragmentManager
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.UiStore
import org.mozilla.fenix.browser.store.BrowserScreenAction.CancelPrivateDownloadsOnPrivateTabsClosedAccepted
import org.mozilla.fenix.browser.store.BrowserScreenAction.ClosingLastPrivateTab
import org.mozilla.fenix.browser.store.BrowserScreenAction.CustomTabColorsUpdated
import org.mozilla.fenix.browser.store.BrowserScreenAction.EnvironmentCleared
import org.mozilla.fenix.browser.store.BrowserScreenAction.EnvironmentRehydrated
import org.mozilla.fenix.browser.store.BrowserScreenAction.PageTranslationStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenAction.ReaderModeStatusUpdated

/**
 * [UiStore] for the browser screen.
 *
 * @param initialState The initial state of the store.
 * @param middleware The middlewares to be applied to this store.
 */
class BrowserScreenStore(
    initialState: BrowserScreenState = BrowserScreenState(),
    middleware: List<Middleware<BrowserScreenState, BrowserScreenAction>> = emptyList(),
) : UiStore<BrowserScreenState, BrowserScreenAction>(
    initialState = initialState,
    reducer = ::reduce,
    middleware = middleware,
) {
    /**
     * The current environment of the browser screen allowing access to various
     * other application features that this integrates with.
     *
     * This is Activity/Fragment lifecycle dependent and should be handled carefully to avoid memory leaks.
     *
     * @property context [Context] used for various system interactions.
     * @property viewLifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     * @property fragmentManager [FragmentManager] to use for showing other fragments.
     */
    data class Environment(
        val context: Context,
        val viewLifecycleOwner: LifecycleOwner,
        val fragmentManager: FragmentManager,
    )
}

private fun reduce(state: BrowserScreenState, action: BrowserScreenAction): BrowserScreenState = when (action) {
    is ClosingLastPrivateTab -> state.copy(
        cancelPrivateDownloadsAccepted = false,
    )

    is CancelPrivateDownloadsOnPrivateTabsClosedAccepted -> state.copy(
        cancelPrivateDownloadsAccepted = true,
    )

    is ReaderModeStatusUpdated -> state.copy(
        readerModeStatus = action.readerModeStatus,
    )

    is PageTranslationStatusUpdated -> state.copy(pageTranslationStatus = action.pageTranslationStatus)

    is CustomTabColorsUpdated -> state.copy(
        customTabColors = action.customTabColors,
    )

    is EnvironmentRehydrated,
    is EnvironmentCleared,
        -> {
        // no-op
        // Expected to be handled in middlewares set by integrators.
        state
    }
}
