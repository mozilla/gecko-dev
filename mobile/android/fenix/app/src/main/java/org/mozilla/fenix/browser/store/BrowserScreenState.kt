/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.store

import mozilla.components.lib.state.State
import org.mozilla.fenix.browser.PageTranslationStatus

/**
 * State of the browser screen.
 *
 * @property cancelPrivateDownloadsAccepted Whether the user has accepted to cancel private downloads.
 * @property pageTranslationStatus Translation status of the current page.
 */
data class BrowserScreenState(
    val cancelPrivateDownloadsAccepted: Boolean = false,
    val pageTranslationStatus: PageTranslationStatus = PageTranslationStatus.NOT_POSSIBLE,
) : State
