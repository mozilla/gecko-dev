/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler

/**
 * Feature implementation for handling fullscreen mode (exiting and back button presses).
 */
open class FullScreenFeature(
    private val store: BrowserStore,
    private val sessionUseCases: SessionUseCases,
    private val tabId: String? = null,
    private val viewportFitChanged: (Int) -> Unit = {},
    private val fullScreenChanged: (Boolean) -> Unit,
) : LifecycleAwareFeature, UserInteractionHandler {
    private var scope: CoroutineScope? = null
    private var observation: Observation = createDefaultObservation()

    /**
     * Returns true if the app is in fullscreen mode.
     */
    val isFullScreen: Boolean
        get() = observation.inFullScreen

    /**
     * Starts the feature and a observer to listen for fullscreen changes.
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.map { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .map { tab -> tab.toObservation() }
                .distinctUntilChanged()
                .collect { observation -> onChange(observation) }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    private fun onChange(observation: Observation) {
        // Immediately update the observation to allow observers of fullscreen / viewport changes
        // to see the new value instead of it being updated after informing about the changes.
        val previousObservation = this.observation
        this.observation = observation

        if (observation.inFullScreen != previousObservation.inFullScreen) {
            fullScreenChanged(observation.inFullScreen)
        }

        if (observation.layoutInDisplayCutoutMode != previousObservation.layoutInDisplayCutoutMode) {
            viewportFitChanged(observation.layoutInDisplayCutoutMode)
        }
    }

    /**
     * To be called when the back button is pressed, so that only fullscreen mode closes.
     *
     * @return Returns true if the fullscreen mode was successfully exited; false if no effect was taken.
     */
    override fun onBackPressed(): Boolean {
        val observation = observation

        if (observation.inFullScreen && observation.tabId != null) {
            sessionUseCases.exitFullscreen(observation.tabId)
            return true
        }

        return false
    }
}

/**
 * Simple holder data class to keep a reference to the last values we observed.
 */
private data class Observation(
    val tabId: String?,
    val inFullScreen: Boolean,
    val layoutInDisplayCutoutMode: Int,
)

private fun SessionState?.toObservation(): Observation {
    return if (this != null) {
        Observation(id, content.fullScreen, content.layoutInDisplayCutoutMode)
    } else {
        createDefaultObservation()
    }
}

private fun createDefaultObservation() = Observation(
    tabId = null,
    inFullScreen = false,
    layoutInDisplayCutoutMode = 0,
)
