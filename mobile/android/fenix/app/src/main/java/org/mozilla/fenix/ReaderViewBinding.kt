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
import org.mozilla.fenix.components.appstate.AppState

/**
 * A binding for observing [AppState.isReaderViewActive] in the [AppStore] and showing/hiding reader view.
 *
 * @param appStore The [AppStore] used to observe [AppState.isReaderViewActive].
 * @param readerMenuController The [ReaderModeController] and toggling the reader view feature.
 */
class ReaderViewBinding(
    private val appStore: AppStore,
    private val readerMenuController: ReaderModeController,
) : AbstractBinding<AppState>(appStore) {

    override suspend fun onState(flow: Flow<AppState>) {
        flow.map { state -> state.isReaderViewActive }
            .distinctUntilChanged()
            .collect { state ->
                when (state) {
                    true -> readerMenuController.showReaderView()
                    false -> readerMenuController.hideReaderView()
                }
            }
    }
}
