/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import org.mozilla.fenix.utils.Settings

/**
 * An interface to persist the state of the link sharing snackbar.
 */
interface SentFromStorage {
    /**
     * Indicates whether the link sharing snackbar has already been shown (once per install).
     */
    var isLinkSharingSettingsSnackbarShown: Boolean

    /**
     * Indicates whether the sent from feature is enabled or not.
     */
    val featureEnabled: Boolean
}

/**
 * Default implementation of [SentFromStorage].
 *
 * @property settings The settings object used to persist the link sharing snackbar state.
 */
class DefaultSentFromStorage(
    val settings: Settings,
) : SentFromStorage {
    override var isLinkSharingSettingsSnackbarShown
        get() = settings.linkSharingSettingsSnackbarShown
        set(value) { settings.linkSharingSettingsSnackbarShown = value }

    override val featureEnabled: Boolean
        get() = settings.whatsappLinkSharingEnabled
}
