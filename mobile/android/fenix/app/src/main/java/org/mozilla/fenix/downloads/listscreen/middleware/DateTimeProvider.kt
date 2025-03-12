/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import java.time.LocalDate

/**
 * Interface for providing date and time information. This is useful for separating the
 * implementation of getting date and time from the rest of the code, making it easier to test
 * the code that uses date and time.
 */
interface DateTimeProvider {

    /**
     * Get the current local date.
     */
    fun currentLocalDate(): LocalDate
}

/**
 * Real Implementation of [DateTimeProvider] that uses the system clock.
 */
class DateTimeProviderImpl : DateTimeProvider {

    /**
     * @see [DateTimeProvider.currentLocalDate]
     */
    override fun currentLocalDate(): LocalDate {
        return LocalDate.now()
    }
}
