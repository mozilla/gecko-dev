/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

/**
 * The current environment in which the browser toolbar is used allowing access to various
 * other application features that the toolbar integrates with.
 */
interface Environment

/**
 * Signals a new valid [Environment] has been set.
 *
 * @property environment The new [Environment].
 */
data class EnvironmentRehydrated(val environment: Environment) : BrowserToolbarAction

/**
 * Signals the current [Environment] is not valid anymore.
 */
data object EnvironmentCleared : BrowserToolbarAction
