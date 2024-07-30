/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test

/**
 * Expects [block] to throw an exception of type [T], and returns the exception.
 */
inline fun <reified T : Throwable> expectException(block: () -> Unit): T =
    try {
        block()
        throw AssertionError("Expected exception to be thrown: ${T::class}")
    } catch (e: Throwable) {
        if (e !is T) {
            throw e
        }
        e
    }
