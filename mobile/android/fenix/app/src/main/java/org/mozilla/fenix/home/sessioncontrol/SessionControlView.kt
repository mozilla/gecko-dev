/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.View
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.ext.showWallpaperOnboardingDialog

/**
 * Shows a list of Home screen views.
 *
 * @param containerView The [View] that is used to initialize the Home recycler view.
 * @param interactor [SessionControlInteractor] which will have delegated to all user interactions.
 */
class SessionControlView(
    containerView: View,
    private val interactor: SessionControlInteractor,
) {

    val view: RecyclerView = containerView as RecyclerView

    init {
        @Suppress("NestedBlockDepth")
        view.apply {
            layoutManager = object : LinearLayoutManager(containerView.context) {
                override fun onLayoutCompleted(state: RecyclerView.State?) {
                    super.onLayoutCompleted(state)

                    if (settings().showWallpaperOnboardingDialog()) {
                        interactor.showWallpapersOnboardingDialog(
                            context.components.appStore.state.wallpaperState,
                        )
                    }

                    // We want some parts of the home screen UI to be rendered first if they are
                    // the most prominent parts of the visible part of the screen.
                    // For this reason, we wait for the home screen recycler view to finish it's
                    // layout and post an update for when it's best for non-visible parts of the
                    // home screen to render itself.
                    containerView.context.components.appStore.dispatch(
                        AppAction.UpdateFirstFrameDrawn(true),
                    )
                }
            }
        }
    }

    fun update(state: AppState, shouldReportMetrics: Boolean = false) {
        if (shouldReportMetrics) interactor.reportSessionMetrics(state)
    }
}
