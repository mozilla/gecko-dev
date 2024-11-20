/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.store

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.UiStore
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.MenuItem

/**
 * Value type that represents the state of the WebCompat Reporter.
 *
 * @property url The URL that is being reported as broken.
 * @property reason Optional param specifying the reason that [url] is broken.
 * @property problemDescription Description of the encountered problem.
 */
data class WebCompatReporterState(
    val url: String = "",
    val reason: BrokenSiteReason? = null,
    val problemDescription: String = "",
) : State {

    /**
     * An enum for the different reasons that a website is broken.
     *
     * @property displayStringId The string ID corresponding to the reason.
     */
    enum class BrokenSiteReason(@StringRes val displayStringId: Int) {
        Slow(
            displayStringId = R.string.webcompat_reporter_reason_slow,
        ),
        Media(
            displayStringId = R.string.webcompat_reporter_reason_media,
        ),
        Content(
            displayStringId = R.string.webcompat_reporter_reason_content,
        ),
        Account(
            displayStringId = R.string.webcompat_reporter_reason_account,
        ),
        AdBlocker(
            displayStringId = R.string.webcompat_reporter_reason_ad_blocker,
        ),
        Other(
            displayStringId = R.string.webcompat_reporter_reason_other,
        ),
    }

    /**
     * Helper function used to obtain the list of dropdown menu items derived from [BrokenSiteReason].
     *
     * @param onDropdownItemClick Callback invoked when the particular dropdown item is selected.
     * @return The list of [MenuItem] to display in the dropdown.
     */
    @Composable
    fun toDropdownItems(
        onDropdownItemClick: (BrokenSiteReason) -> Unit,
    ): List<MenuItem> {
        return BrokenSiteReason.entries.map { reason ->
            MenuItem(
                title = stringResource(id = reason.displayStringId),
                isChecked = this.reason == reason,
                onClick = {
                    onDropdownItemClick(reason)
                },
            )
        }
    }

    /**
     * Whether the URL text field has an error.
     */
    val hasUrlTextError: Boolean
        get() = url.isEmpty()
}

/**
 * [Action] implementation related to [WebCompatReporterStore].
 */
sealed class WebCompatReporterAction : Action {

    /**
     * Dispatched when the URL field is updated.
     *
     * @property newUrl The updated URL field.
     */
    data class BrokenSiteChanged(val newUrl: String) : WebCompatReporterAction()

    /**
     * Dispatched when the broken site reason is updated.
     *
     * @property newReason The updated broken site reason.
     */
    data class ReasonChanged(val newReason: WebCompatReporterState.BrokenSiteReason) : WebCompatReporterAction()

    /**
     * Dispatched when the problem description is updated.
     *
     * @property newProblemDescription The updated problem description.
     */
    data class ProblemDescriptionChanged(val newProblemDescription: String) : WebCompatReporterAction()

    /**
     * Dispatched when the user requests to send the WebCompat report.
     */
    data object SendReportClicked : WebCompatReporterAction()

    /**
     * Dispatched when the user requests to send more info.
     */
    data object SendMoreInfoClicked : WebCompatReporterAction()

    /**
     * Dispatched when the user requests to cancel the report.
     */
    data object CancelClicked : WebCompatReporterAction()

    /**
     * Dispatched when the user requests to navigate to the previous page.
     */
    data object BackPressed : WebCompatReporterAction()
}

private fun reduce(
    state: WebCompatReporterState,
    action: WebCompatReporterAction,
): WebCompatReporterState = when (action) {
    is WebCompatReporterAction.BrokenSiteChanged -> state.copy(url = action.newUrl)
    is WebCompatReporterAction.ProblemDescriptionChanged -> state.copy(
        problemDescription = action.newProblemDescription,
    )
    is WebCompatReporterAction.ReasonChanged -> state.copy(reason = action.newReason)
    WebCompatReporterAction.SendMoreInfoClicked -> state
    WebCompatReporterAction.SendReportClicked -> state
    WebCompatReporterAction.BackPressed -> state
    WebCompatReporterAction.CancelClicked -> state
}

/**
 * A [UiStore] that holds the [WebCompatReporterState] for the WebCompat Reporter and reduces
 * [WebCompatReporterAction]s dispatched to the store.
 */
class WebCompatReporterStore(
    initialState: WebCompatReporterState = WebCompatReporterState(),
) : UiStore<WebCompatReporterState, WebCompatReporterAction>(
    initialState,
    ::reduce,
)
