/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.core

/**
 * Wrapper class for a UI interaction with text and a callback when invoked.
 *
 * @property label The action's text to display.
 * @property onClick Called when the user invokes the action.
 */
data class Action(
    val label: String,
    val onClick: () -> Unit,
)
