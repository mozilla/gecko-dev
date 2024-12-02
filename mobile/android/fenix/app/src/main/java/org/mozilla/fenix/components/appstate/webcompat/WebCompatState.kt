/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.webcompat

/**
 * State for WebCompat Reporter feature that's required to live the lifetime of a session.
 *
 * @property tabUrl The URL that was active at the time the user was last in the Web Compat Reporter feature.
 * @property enteredUrl The URL that was in the URL text field at the time the user was last
 * in the Web Compat Reporter feature.
 * @property reason Optional param specifying the reason that [tabUrl] is broken.
 * @property problemDescription Description of the encountered problem.
 */
data class WebCompatState(
    val tabUrl: String = "",
    val enteredUrl: String = "",
    val reason: String? = null,
    val problemDescription: String = "",
)
