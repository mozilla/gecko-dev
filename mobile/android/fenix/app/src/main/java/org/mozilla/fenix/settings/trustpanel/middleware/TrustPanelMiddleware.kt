/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.middleware

import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.trackingprotection.TrackerBuckets
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory

/**
 * [Middleware] implementation for handling [TrustPanelAction] and managing the [TrustPanelState] for the menu
 * dialog.
 *
 * @param appStore The [AppStore] used to dispatch actions to display a snackbar.
 * @param engine The browser [Engine] used to clear site data.
 * @param publicSuffixList The [PublicSuffixList] used to obtain the base domain of the current site.
 * @param sessionUseCases [SessionUseCases] used to reload the page after toggling tracking protection.
 * @param trackingProtectionUseCases [TrackingProtectionUseCases] used to add/remove sites from the
 * tracking protection exceptions list.
 * @param onDismiss Callback invoked to dismiss the trust panel.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
@Suppress("LongParameterList")
class TrustPanelMiddleware(
    private val appStore: AppStore,
    private val engine: Engine,
    private val publicSuffixList: PublicSuffixList,
    private val sessionUseCases: SessionUseCases,
    private val trackingProtectionUseCases: TrackingProtectionUseCases,
    private val onDismiss: suspend () -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<TrustPanelState, TrustPanelAction> {

    override fun invoke(
        context: MiddlewareContext<TrustPanelState, TrustPanelAction>,
        next: (TrustPanelAction) -> Unit,
        action: TrustPanelAction,
    ) {
        val currentState = context.state
        val store = context.store

        when (action) {
            is TrustPanelAction.ClearSiteData -> clearSiteData(currentState)
            is TrustPanelAction.RequestClearSiteDataDialog -> requestClearSiteDataDialog(currentState, store)
            TrustPanelAction.ToggleTrackingProtection -> toggleTrackingProtection(currentState)
            is TrustPanelAction.UpdateTrackersBlocked,
            -> updateTrackersBlocked(currentState, action.trackerLogs, store)

            else -> Unit
        }

        next(action)
    }

    private fun toggleTrackingProtection(
        currentState: TrustPanelState,
    ) = scope.launch {
        currentState.sessionState?.let { session ->
            if (currentState.isTrackingProtectionEnabled) {
                trackingProtectionUseCases.addException(session.id)
            } else {
                trackingProtectionUseCases.removeException(session.id)
            }

            sessionUseCases.reload.invoke(session.id)
        }
    }

    private fun updateTrackersBlocked(
        currentState: TrustPanelState,
        newTrackersBlocked: List<TrackerLog>,
        store: Store<TrustPanelState, TrustPanelAction>,
    ) = scope.launch {
        currentState.bucketedTrackers.updateIfNeeded(newTrackersBlocked)
        store.dispatch(
            TrustPanelAction.UpdateNumberOfTrackersBlocked(currentState.bucketedTrackers.numberOfTrackersBlocked()),
        )
    }

    private fun requestClearSiteDataDialog(
        currentState: TrustPanelState,
        store: Store<TrustPanelState, TrustPanelAction>,
    ) = scope.launch {
        val host = currentState.sessionState?.content?.url?.toUri()?.host.orEmpty()
        val domain = publicSuffixList.getPublicSuffixPlusOne(host).await()

        domain?.let { baseDomain ->
            store.dispatch(TrustPanelAction.UpdateBaseDomain(baseDomain))
            store.dispatch(TrustPanelAction.Navigate.ClearSiteDataDialog)
        }
    }

    private fun clearSiteData(
        currentState: TrustPanelState,
    ) = scope.launch {
        currentState.baseDomain?.let {
            engine.clearData(
                host = it,
                data = Engine.BrowsingData.select(
                    Engine.BrowsingData.AUTH_SESSIONS,
                    Engine.BrowsingData.ALL_SITE_DATA,
                ),
            )

            appStore.dispatch(AppAction.SiteDataCleared)
        }

        onDismiss()
    }
}

private fun TrackerBuckets.numberOfTrackersBlocked() = TrackingProtectionCategory.entries
    .sumOf { trackingProtectionCategory -> this.get(trackingProtectionCategory, true).size }
