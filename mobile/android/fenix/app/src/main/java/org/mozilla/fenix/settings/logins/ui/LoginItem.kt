/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import java.net.URI

/**
 *  An item representing a saved login
 *
 *  @property guid The id of the login.
 *  @property url The site where the login is created.
 *  @property username The username of the login.
 *  @property password The password of the login.
 *  @property timeLastUsed The time in milliseconds when the login was last used.
 */
data class LoginItem(
    val guid: String,
    val url: String,
    val username: String,
    val password: String,
    val timeLastUsed: Long = 0L,
)

internal fun LoginItem.getDomainName(): String = URI(url).host.removePrefix("www.")
