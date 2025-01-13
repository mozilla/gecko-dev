/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.middleware

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.webcompat.store.WebCompatReporterAction
import org.mozilla.fenix.webcompat.store.WebCompatReporterState
import org.mozilla.fenix.webcompat.store.WebCompatReporterStore

/**
 * [Middleware] that handles navigation events for the WebCompat Reporter.
 */
class WebCompatReporterNavigationMiddleware : Middleware<WebCompatReporterState, WebCompatReporterAction> {

    override fun invoke(
        context: MiddlewareContext<WebCompatReporterState, WebCompatReporterAction>,
        next: (WebCompatReporterAction) -> Unit,
        action: WebCompatReporterAction,
    ) {
        next(action)

        when (action) {
            is WebCompatReporterAction.NavigationAction ->
                (context.store as WebCompatReporterStore).emitNavAction(
                    action = action,
                )
            else -> {}
        }
    }
}
