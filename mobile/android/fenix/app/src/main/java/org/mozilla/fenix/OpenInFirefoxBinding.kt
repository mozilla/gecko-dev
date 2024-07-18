/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Intent
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * A binding for observing [AppState.openInFirefoxRequested] in the [AppStore] and opening the tab in the browser.
 *
 * @param activity The [HomeActivity] used to switch to the actual browser.
 * @param appStore The [AppStore] used to observe [AppState.openInFirefoxRequested].
 * @param customTabSessionId Optional custom tab session ID if navigating from a custom tab or null
 * if the selected session should be used.
 * @param customTabsUseCases The [CustomTabsUseCases] used to turn the session into a regular tab and select it.
 * @param openInFenixIntent The [Intent] used to open the tab in the browser.
 * @param sessionFeature The [SessionFeature] used to release the session from the EngineView.
 */
class OpenInFirefoxBinding(
    private val activity: HomeActivity,
    private val appStore: AppStore,
    private val customTabSessionId: String?,
    private val customTabsUseCases: CustomTabsUseCases,
    private val openInFenixIntent: Intent,
    private val sessionFeature: ViewBoundFeatureWrapper<SessionFeature>,
) : AbstractBinding<AppState>(appStore) {

    override suspend fun onState(flow: Flow<AppState>) {
        flow.map { state -> state.openInFirefoxRequested }
            .distinctUntilChanged()
            .collect { state ->
                when (state) {
                    true -> {
                        customTabSessionId?.let {
                            // Stop the SessionFeature from updating the EngineView and let it release the session
                            // from the EngineView so that it can immediately be rendered by a different view once
                            // we switch to the actual browser.
                            sessionFeature.get()?.release()

                            // Turn this Session into a regular tab and then select it
                            customTabsUseCases.migrate(customTabSessionId, select = true)

                            // Switch to the actual browser which should now display our new selected session
                            activity.startActivity(
                                openInFenixIntent.apply {
                                    // We never want to launch the browser in the same task as the external app
                                    // activity. So we force a new task here. IntentReceiverActivity will do the
                                    // right thing and take care of routing to an already existing browser and avoid
                                    // cloning a new one.
                                    flags = flags or Intent.FLAG_ACTIVITY_NEW_TASK
                                },
                            )

                            // Close this activity (and the task) since it is no longer displaying any session
                            activity.finishAndRemoveTask()
                            appStore.dispatch(AppAction.OpenInFirefoxFinished)
                        }
                    }

                    false -> Unit
                }
            }
    }
}
