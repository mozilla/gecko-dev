/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.store

import mozilla.components.lib.state.Action
import org.mozilla.fenix.browser.PageTranslationStatus
import org.mozilla.fenix.browser.ReaderModeStatus

/**
 * Actions related to the browser screen.
 */
sealed class BrowserScreenAction : Action {
    /**
     * [Action] for when the last private tab is about to be closed.
     *
     * @property tabId Id of the tab that was just closed.
     * @property inProgressPrivateDownloads Number of in-progress downloads in private tabs
     */
    data class ClosingLastPrivateTab(
        val tabId: String,
        val inProgressPrivateDownloads: Int,
    ) : BrowserScreenAction()

    /**
     * [Action] for when the user has accepted the cancellation of private downloads
     * in the scenario of closing all private tabs.
     */
    data object CancelPrivateDownloadsOnPrivateTabsClosedAccepted : BrowserScreenAction()

    /**
     * [Action] for when the reader mode status of a page has been updated.
     *
     * @property readerModeStatus The new reader mode status of the current page.
     */
    data class ReaderModeStatusUpdated(
        val readerModeStatus: ReaderModeStatus,
    ) : BrowserScreenAction()

    /**
     * [Action] for when the translation status of a page has been updated.
     *
     * @property pageTranslationStatus The new translation status of the current page.
     */
    data class PageTranslationStatusUpdated(
        val pageTranslationStatus: PageTranslationStatus,
    ) : BrowserScreenAction()
}
