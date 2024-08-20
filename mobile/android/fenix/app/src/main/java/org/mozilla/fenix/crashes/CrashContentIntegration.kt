/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.content.Context
import android.view.ViewGroup.MarginLayoutParams
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.utils.Settings

/**
 * Helper for observing [BrowserStore] and show an in-app crash reporter for tabs with content crashes.
 *
 * @param context [Context] of the host fragment that is used for accessing settings and for using extension functions.
 * @param browserStore [BrowserStore] observed for any changes related to [EngineState.crashed].
 * @param appStore [AppStore] that tracks all content crashes in the current app session until the user
 * decides to either send or dismiss all crash reports.
 * @param toolbar [BrowserToolbar] that will be expanded when showing the in-app crash reporter.
 * @param crashReporterView [CrashContentView] which will be shown if the current tab is marked as crashed.
 * @param components [Components] allowing interactions with other app features.
 * @param settings [Settings] allowing to check whether crash reporting is enabled or not.
 * @param navController [NavController] used to navigate to other parts of the app.
 * @param sessionId [String] Id of the tab or custom tab which should be observed for [EngineState.crashed]
 * depending on which [crashReporterView] will be shown or hidden.
 */

@Suppress("LongParameterList")
class CrashContentIntegration(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val appStore: AppStore,
    private val toolbar: BrowserToolbar,
    private val crashReporterView: CrashContentView,
    private val components: Components,
    private val settings: Settings,
    private val navController: NavController,
    private val sessionId: String?,
) : LifecycleAwareFeature {

    @VisibleForTesting
    lateinit var scope: CoroutineScope

    override fun start() {
        scope = MainScope().apply {
            launch {
                browserStore.flow()
                    .mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(sessionId) }
                    .distinctUntilChangedBy { tab -> tab.engineState.crashed }
                    .collect { tab ->
                        if (tab.engineState.crashed) {
                            toolbar.expand()

                            crashReporterView.apply {
                                val controller = CrashReporterController(
                                    sessionId = tab.id,
                                    currentNumberOfTabs = if (tab.content.private) {
                                        browserStore.state.privateTabs.size
                                    } else {
                                        browserStore.state.normalTabs.size
                                    },
                                    components = components,
                                    settings = settings,
                                    navController = navController,
                                    appStore = appStore,
                                )

                                show(controller)

                                updateVerticalMargins()
                            }
                        } else {
                            crashReporterView.hide()
                        }
                    }
            }

            launch {
                appStore.flow()
                    .distinctUntilChangedBy { it.orientation }
                    .map { it.orientation }
                    .collect {
                        updateVerticalMargins()
                    }
            }
        }
    }

    override fun stop() {
        scope.cancel()
    }

    @VisibleForTesting
    internal fun updateVerticalMargins() {
        with(crashReporterView.layoutParams as MarginLayoutParams) {
            val includeTabStrip = sessionId == null && context.isTabStripEnabled()
            topMargin = settings.getTopToolbarHeight(includeTabStrip)
            bottomMargin = settings.getBottomToolbarHeight(context)
        }
    }
}
