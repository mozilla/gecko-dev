/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Webcompatreporting

/**
 * A [Middleware] for recording telemetry based on [WebCompatReporterAction]s that are dispatch to the
 * [WebCompatReporterStore].
 */
class WebCompatReporterTelemetryMiddleware :
    Middleware<WebCompatReporterState, WebCompatReporterAction> {

    override fun invoke(
        context: MiddlewareContext<WebCompatReporterState, WebCompatReporterAction>,
        next: (WebCompatReporterAction) -> Unit,
        action: WebCompatReporterAction,
    ) {
        next(action)

        when (action) {
            is WebCompatReporterAction.ReasonChanged -> {
                Webcompatreporting.reasonDropdown.set(action.newReason.name)
            }

            WebCompatReporterAction.SendMoreInfoClicked -> {
                Webcompatreporting.sendMoreInfo.record(NoExtras())
            }

            WebCompatReporterAction.SendReportClicked -> {
                Webcompatreporting.send.record(NoExtras())
            }
            else -> {}
        }
    }
}
