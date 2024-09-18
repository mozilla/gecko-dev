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
 * Data class containing the boolean that dictates the need of displaying the
 * biometric authentication prompt
 */
data class BiometricAuthenticationNeededInfo(
    var shouldAuthenticate: Boolean = true,
)
