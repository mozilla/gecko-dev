/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import org.mozilla.fenix.components.appstate.AppState

/**
 * Shows a list of Home screen views.
 *
 * @param interactor [SessionControlInteractor] which will have delegated to all user interactions.
 */
class SessionControlView(
    private val interactor: SessionControlInteractor,
) {
    fun update(state: AppState, shouldReportMetrics: Boolean = false) {
        if (shouldReportMetrics) interactor.reportSessionMetrics(state)
    }
}
