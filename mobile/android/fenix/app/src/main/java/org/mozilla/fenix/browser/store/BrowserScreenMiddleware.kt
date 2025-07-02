/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.store

import android.view.Gravity
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.feature.downloads.ui.DownloadCancelDialogFragment
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.store.BrowserScreenAction.CancelPrivateDownloadsOnPrivateTabsClosedAccepted
import org.mozilla.fenix.browser.store.BrowserScreenAction.ClosingLastPrivateTab
import org.mozilla.fenix.browser.store.BrowserScreenAction.EnvironmentCleared
import org.mozilla.fenix.browser.store.BrowserScreenAction.EnvironmentRehydrated
import org.mozilla.fenix.browser.store.BrowserScreenStore.Environment
import org.mozilla.fenix.components.toolbar.BrowserToolbarMiddleware
import org.mozilla.fenix.theme.ThemeManager

/**
 * [Middleware] responsible for handling actions related to the browser screen.
 *
 * @param crashReporter [CrashReporter] for recording crashes.
 */
class BrowserScreenMiddleware(
    private val crashReporter: CrashReporter,
) : Middleware<BrowserScreenState, BrowserScreenAction> {
    @VisibleForTesting
    internal var environment: Environment? = null

    override fun invoke(
        context: MiddlewareContext<BrowserScreenState, BrowserScreenAction>,
        next: (BrowserScreenAction) -> Unit,
        action: BrowserScreenAction,
    ) {
        when (action) {
            is EnvironmentRehydrated -> {
                next(action)

                environment = action.environment
            }

            is EnvironmentCleared -> {
                next(action)

                environment = null
            }

            is ClosingLastPrivateTab -> {
                next(action)

                showCancelledDownloadWarning(
                    store = context.store,
                    downloadCount = action.inProgressPrivateDownloads,
                    tabId = action.tabId,
                )
            }

            else -> next(action)
        }
    }

    private fun showCancelledDownloadWarning(
        store: Store<BrowserScreenState, BrowserScreenAction>,
        downloadCount: Int,
        tabId: String?,
    ) {
        val environment = environment ?: return

        crashReporter.recordCrashBreadcrumb(
            Breadcrumb("DownloadCancelDialogFragment shown in browser screen"),
        )
        val dialog = DownloadCancelDialogFragment.newInstance(
            downloadCount = downloadCount,
            tabId = tabId,
            source = null,
            promptStyling = DownloadCancelDialogFragment.PromptStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true,
                positiveButtonBackgroundColor = ThemeManager.resolveAttribute(
                    R.attr.accent,
                    environment.context,
                ),
                positiveButtonTextColor = ThemeManager.resolveAttribute(
                    R.attr.textOnColorPrimary,
                    environment.context,
                ),
                positiveButtonRadius = environment.context.resources.getDimensionPixelSize(
                    R.dimen.tab_corner_radius,
                ).toFloat(),
            ),

            onPositiveButtonClicked = { _, _ ->
                store.dispatch(CancelPrivateDownloadsOnPrivateTabsClosedAccepted)
            },
        )
        dialog.show(environment.fragmentManager, CANCEL_PRIVATE_DOWNLOADS_DIALOG_FRAGMENT_TAG)
    }

    /**
     * Static functionalities of the [BrowserToolbarMiddleware].
     */
    companion object {
        @VisibleForTesting
        internal const val CANCEL_PRIVATE_DOWNLOADS_DIALOG_FRAGMENT_TAG = "CANCEL_PRIVATE_DOWNLOADS_DIALOG_FRAGMENT_TAG"
    }
}
