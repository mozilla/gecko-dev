/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.readerview.ReaderViewState.Active
import org.mozilla.fenix.components.appstate.readerview.ReaderViewState.Dismiss
import org.mozilla.fenix.components.appstate.readerview.ReaderViewState.None
import org.mozilla.fenix.components.appstate.readerview.ReaderViewState.ShowControls

/**
 * A binding for observing [AppState.readerViewState] in the [AppStore] and toggling the
 * reader view feature and controls.
 *
 * @param appStore The [AppStore] used to observe [AppState.isReaderViewActive].
 * @param readerMenuController The [ReaderModeController] that will used for toggling the reader
 * view feature and controls.
 */
class ReaderViewBinding(
    private val appStore: AppStore,
    private val readerMenuController: ReaderModeController,
) : AbstractBinding<AppState>(appStore) {

    override suspend fun onState(flow: Flow<AppState>) {
        flow.map { state -> state.readerViewState }
            .distinctUntilChanged()
            .collect { state ->
                when (state) {
                    Active -> {
                        readerMenuController.showReaderView()
                        appStore.dispatch(AppAction.ReaderViewAction.Reset)
                    }

                    Dismiss -> {
                        readerMenuController.hideReaderView()
                        appStore.dispatch(AppAction.ReaderViewAction.Reset)
                    }

                    ShowControls -> {
                        readerMenuController.showControls()
                        appStore.dispatch(AppAction.ReaderViewAction.Reset)
                    }

                    None -> Unit
                }
            }
    }
}
