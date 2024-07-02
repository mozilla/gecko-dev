/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.snackbar

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.ui.widgets.SnackbarDelegate
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.appstate.AppAction.SnackbarAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.snackbar.SnackbarState

/**
 * A binding for observing the [SnackbarState] in the [AppStore] and displaying the snackbar.
 *
 * @param browserStore The [BrowserStore] used to get the current session.
 * @param appStore The [AppStore] used to observe the [SnackbarState].
 * @param snackbarDelegate The [SnackbarDelegate] used to display a snackbar.
 * @param customTabSessionId Optional custom tab session ID if navigating from a custom tab or null
 * if the selected session should be used.
 */
class SnackbarBinding(
    private val browserStore: BrowserStore,
    private val appStore: AppStore,
    private val snackbarDelegate: FenixSnackbarDelegate,
    private val customTabSessionId: String?,
) : AbstractBinding<AppState>(appStore) {

    private val currentSession
        get() = browserStore.state.findCustomTabOrSelectedTab(customTabSessionId)

    override suspend fun onState(flow: Flow<AppState>) {
        flow.map { state -> state.snackbarState }
            .distinctUntilChanged()
            .collect { state ->
                when (state) {
                    is SnackbarState.Dismiss -> {
                        snackbarDelegate.dismiss()
                        appStore.dispatch(SnackbarAction.Reset)
                    }

                    is SnackbarState.TranslationInProgress -> {
                        if (currentSession?.id != state.sessionId) {
                            return@collect
                        }

                        snackbarDelegate.show(
                            text = R.string.translation_in_progress_snackbar,
                            duration = FenixSnackbar.LENGTH_INDEFINITE,
                        )

                        appStore.dispatch(SnackbarAction.SnackbarShown)
                    }

                    SnackbarState.None -> Unit
                }
            }
    }
}
