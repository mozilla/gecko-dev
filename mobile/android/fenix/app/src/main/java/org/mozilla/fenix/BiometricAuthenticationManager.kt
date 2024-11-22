/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

/**
 *  A single source for the biometric authentication need on the secure fragments
 */
object BiometricAuthenticationManager {
    val biometricAuthenticationNeededInfo = BiometricAuthenticationNeededInfo()
}

/**
 * Data class containing a boolean that dictates the need of displaying the
 * biometric authentication prompt and the authentication status
 */
data class BiometricAuthenticationNeededInfo(
    var shouldShowAuthenticationPrompt: Boolean = true,
    var authenticationStatus: AuthenticationStatus = AuthenticationStatus.NOT_AUTHENTICATED,
)

/**
 * Enum class defining the 3 possible states of the biometric authentication
 */
enum class AuthenticationStatus {
    AUTHENTICATED, NOT_AUTHENTICATED, AUTHENTICATION_IN_PROGRESS
}
