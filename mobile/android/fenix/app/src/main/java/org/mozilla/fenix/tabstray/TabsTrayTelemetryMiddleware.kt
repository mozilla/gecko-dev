/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.TabsTray

/**
 * Middleware that records telemetry events for the Tabs Tray feature.
 */
class TabsTrayTelemetryMiddleware : Middleware<TabsTrayState, TabsTrayAction> {

    private var shouldReportInactiveTabMetrics: Boolean = true

    override fun invoke(
        context: MiddlewareContext<TabsTrayState, TabsTrayAction>,
        next: (TabsTrayAction) -> Unit,
        action: TabsTrayAction,
    ) {
        next(action)

        when (action) {
            is TabsTrayAction.UpdateInactiveTabs -> {
                if (shouldReportInactiveTabMetrics) {
                    shouldReportInactiveTabMetrics = false

                    TabsTray.hasInactiveTabs.record(TabsTray.HasInactiveTabsExtra(action.tabs.size))
                    Metrics.inactiveTabsCount.set(action.tabs.size.toLong())
                }
            }
            is TabsTrayAction.EnterSelectMode -> {
                TabsTray.enterMultiselectMode.record(TabsTray.EnterMultiselectModeExtra(false))
            }
            is TabsTrayAction.AddSelectTab -> {
                TabsTray.enterMultiselectMode.record(TabsTray.EnterMultiselectModeExtra(true))
            }
            is TabsTrayAction.TabAutoCloseDialogShown -> {
                TabsTray.autoCloseSeen.record(NoExtras())
            }
            is TabsTrayAction.ShareAllNormalTabs -> {
                TabsTray.shareAllTabs.record(NoExtras())
            }
            is TabsTrayAction.ShareAllPrivateTabs -> {
                TabsTray.shareAllTabs.record(NoExtras())
            }
            is TabsTrayAction.CloseAllNormalTabs -> {
                TabsTray.closeAllTabs.record(NoExtras())
            }
            is TabsTrayAction.CloseAllPrivateTabs -> {
                TabsTray.closeAllTabs.record(NoExtras())
            }
            else -> {
                // no-op
            }
        }
    }
}
