/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.store

import mozilla.components.browser.state.state.SessionState
import mozilla.components.lib.state.State

/**
 * Value type that represents the state of the unified trust panel.
 *
 * @property isTrackingProtectionEnabled Flag indicating whether enhanced tracking protection is enabled.
 * @property sessionState The [SessionState] of the current tab.
 */
data class TrustPanelState(
    val isTrackingProtectionEnabled: Boolean = true,
    val sessionState: SessionState? = null,
) : State
