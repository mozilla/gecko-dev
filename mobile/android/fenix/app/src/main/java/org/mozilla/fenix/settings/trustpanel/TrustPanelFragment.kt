/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.core.content.ContextCompat
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.view.setNavigationBarColorCompat
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.components.menu.compose.MenuDialogBottomSheet
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelMiddleware
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelNavigationMiddleware
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelTelemetryMiddleware
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore
import org.mozilla.fenix.settings.trustpanel.ui.CLEAR_SITE_DATA_DIALOG_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.CONNECTION_SECURITY_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.ClearSiteDataDialog
import org.mozilla.fenix.settings.trustpanel.ui.ConnectionSecurityPanel
import org.mozilla.fenix.settings.trustpanel.ui.PROTECTION_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.ProtectionPanel
import org.mozilla.fenix.settings.trustpanel.ui.TRACKERS_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.TRACKER_CATEGORY_DETAILS_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.TrackerCategoryDetailsPanel
import org.mozilla.fenix.settings.trustpanel.ui.TrackersBlockedPanel
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.trackingprotection.TrackerBuckets

/**
 * A bottom sheet dialog fragment displaying the unified trust panel.
 */
class TrustPanelFragment : BottomSheetDialogFragment() {

    private val args by navArgs<TrustPanelFragmentArgs>()

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                val safeActivity = activity ?: return@setOnShowListener
                val browsingModeManager = (safeActivity as HomeActivity).browsingModeManager

                val navigationBarColor = if (browsingModeManager.mode.isPrivate) {
                    ContextCompat.getColor(context, R.color.fx_mobile_private_layer_color_3)
                } else {
                    ContextCompat.getColor(context, R.color.fx_mobile_layer_color_3)
                }

                window?.setNavigationBarColorCompat(navigationBarColor)

                val bottomSheet = findViewById<View?>(R.id.design_bottom_sheet)
                bottomSheet?.setBackgroundResource(android.R.color.transparent)

                val behavior = BottomSheetBehavior.from(bottomSheet)
                behavior.peekHeight = resources.displayMetrics.heightPixels
                behavior.state = BottomSheetBehavior.STATE_EXPANDED
            }
        }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            FirefoxTheme {
                MenuDialogBottomSheet(
                    onRequestDismiss = ::dismiss,
                    handlebarContentDescription = "",
                ) {
                    val components = components
                    val trackingProtectionUseCases = components.useCases.trackingProtectionUseCases

                    val navHostController = rememberNavController()
                    val coroutineScope = rememberCoroutineScope()
                    val store = remember {
                        TrustPanelStore(
                            initialState = TrustPanelState(
                                isTrackingProtectionEnabled = args.isTrackingProtectionEnabled,
                                sessionState = components.core.store.state.findTabOrCustomTab(args.sessionId),
                            ),
                            middleware = listOf(
                                TrustPanelMiddleware(
                                    appStore = components.appStore,
                                    engine = components.core.engine,
                                    publicSuffixList = components.publicSuffixList,
                                    sessionUseCases = components.useCases.sessionUseCases,
                                    trackingProtectionUseCases = trackingProtectionUseCases,
                                    onDismiss = {
                                        withContext(Dispatchers.Main) {
                                            this@TrustPanelFragment.dismiss()
                                        }
                                    },
                                    scope = coroutineScope,
                                ),
                                TrustPanelNavigationMiddleware(
                                    navController = findNavController(),
                                    navHostController = navHostController,
                                    privacySecurityPrefKey = requireContext().getString(
                                        R.string.pref_key_privacy_security_category,
                                    ),
                                    scope = coroutineScope,
                                ),
                                TrustPanelTelemetryMiddleware(),
                            ),
                        )
                    }

                    val baseDomain by store.observeAsState(initialValue = null) { state ->
                        state.baseDomain
                    }
                    val isTrackingProtectionEnabled by store.observeAsState(initialValue = false) { state ->
                        state.isTrackingProtectionEnabled
                    }
                    val numberOfTrackersBlocked by store.observeAsState(initialValue = 0) { state ->
                        state.numberOfTrackersBlocked
                    }
                    val bucketedTrackers by store.observeAsState(initialValue = TrackerBuckets()) { state ->
                        state.bucketedTrackers
                    }
                    val detailedTrackerCategory by store.observeAsState(initialValue = null) { state ->
                        state.detailedTrackerCategory
                    }
                    val sessionState by store.observeAsState(initialValue = null) { state ->
                        state.sessionState
                    }

                    observeTrackersChange(components.core.store) {
                        trackingProtectionUseCases.fetchTrackingLogs(
                            tabId = args.sessionId,
                            onSuccess = { trackerLogs ->
                                store.dispatch(TrustPanelAction.UpdateTrackersBlocked(trackerLogs))
                            },
                            onError = {
                                Logger.error("TrackingProtectionUseCases - fetchTrackingLogs onError", it)
                            },
                        )
                    }

                    NavHost(
                        navController = navHostController,
                        startDestination = PROTECTION_PANEL_ROUTE,
                    ) {
                        composable(route = PROTECTION_PANEL_ROUTE) {
                            ProtectionPanel(
                                url = args.url,
                                title = args.title,
                                icon = sessionState?.content?.icon,
                                isSecured = args.isSecured,
                                isTrackingProtectionEnabled = isTrackingProtectionEnabled,
                                numberOfTrackersBlocked = numberOfTrackersBlocked,
                                onTrackerBlockedMenuClick = {
                                    store.dispatch(TrustPanelAction.Navigate.TrackersPanel)
                                },
                                onTrackingProtectionToggleClick = {
                                    store.dispatch(TrustPanelAction.ToggleTrackingProtection)
                                },
                                onClearSiteDataMenuClick = {
                                    store.dispatch(TrustPanelAction.RequestClearSiteDataDialog)
                                },
                                onConnectionSecurityClick = {
                                    store.dispatch(TrustPanelAction.Navigate.ConnectionSecurityPanel)
                                },
                                onPrivacySecuritySettingsClick = {
                                    store.dispatch(TrustPanelAction.Navigate.PrivacySecuritySettings)
                                },
                            )
                        }

                        composable(route = TRACKERS_PANEL_ROUTE) {
                            TrackersBlockedPanel(
                                title = args.title,
                                numberOfTrackersBlocked = numberOfTrackersBlocked,
                                bucketedTrackers = bucketedTrackers,
                                onTrackerCategoryClick = { detailedTrackerCategory ->
                                    store.dispatch(
                                        TrustPanelAction.UpdateDetailedTrackerCategory(detailedTrackerCategory),
                                    )
                                    store.dispatch(TrustPanelAction.Navigate.TrackerCategoryDetailsPanel)
                                },
                                onBackButtonClick = {
                                    store.dispatch(TrustPanelAction.Navigate.Back)
                                },
                            )
                        }

                        composable(route = TRACKER_CATEGORY_DETAILS_PANEL_ROUTE) {
                            TrackerCategoryDetailsPanel(
                                title = args.title,
                                isTotalCookieProtectionEnabled = components.settings.enabledTotalCookieProtection,
                                detailedTrackerCategory = detailedTrackerCategory,
                                bucketedTrackers = bucketedTrackers,
                                onBackButtonClick = {
                                    store.dispatch(TrustPanelAction.Navigate.Back)
                                },
                            )
                        }

                        composable(route = CONNECTION_SECURITY_PANEL_ROUTE) {
                            ConnectionSecurityPanel(
                                title = args.title,
                                isSecured = args.isSecured,
                                certificateName = args.certificateName,
                                onBackButtonClick = {
                                    store.dispatch(TrustPanelAction.Navigate.Back)
                                },
                            )
                        }

                        composable(route = CLEAR_SITE_DATA_DIALOG_ROUTE) {
                            ClearSiteDataDialog(
                                baseDomain = baseDomain ?: "",
                                onClearSiteDataClick = {
                                    store.dispatch(TrustPanelAction.ClearSiteData)
                                },
                                onCancelClick = { ::dismiss.invoke() },
                            )
                        }
                    }
                }
            }
        }
    }

    private fun observeTrackersChange(store: BrowserStore, onChange: (SessionState) -> Unit) {
        consumeFlow(store) { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTab(args.sessionId) }
                .ifAnyChanged { tab -> arrayOf(tab.trackingProtection.blockedTrackers) }
                .collect(onChange)
        }
    }
}
