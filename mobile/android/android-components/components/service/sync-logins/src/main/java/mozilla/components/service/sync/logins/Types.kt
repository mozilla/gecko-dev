/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry

// Convert between application-services data classes and the ones in concept.storage.

/**
 * Convert A-S Login into A-C [Login].
 */
fun mozilla.appservices.logins.Login.toLogin() = Login(
    guid = id,
    origin = origin,
    username = username,
    password = password,
    formActionOrigin = formActionOrigin,
    httpRealm = httpRealm,
    usernameField = usernameField,
    passwordField = passwordField,
    timesUsed = timesUsed,
    timeCreated = timeCreated,
    timeLastUsed = timeLastUsed,
    timePasswordChanged = timePasswordChanged,
)

/**
 * Convert A-C [LoginEntry] into A-S LoginEntry.
 */
fun LoginEntry.toLoginEntry() = mozilla.appservices.logins.LoginEntry(
    origin = origin,
    formActionOrigin = formActionOrigin,
    httpRealm = httpRealm,
    usernameField = usernameField,
    passwordField = passwordField,
    username = username,
    password = password,
)

/**
 * Convert A-C [Login] into A-S Login.
 */
fun Login.toLogin() = mozilla.appservices.logins.Login(
    id = guid,
    timesUsed = timesUsed,
    timeCreated = timeCreated,
    timeLastUsed = timeLastUsed,
    timePasswordChanged = timePasswordChanged,
    origin = origin,
    formActionOrigin = formActionOrigin,
    httpRealm = httpRealm,
    usernameField = usernameField,
    passwordField = passwordField,
    username = username,
    password = password,
)
